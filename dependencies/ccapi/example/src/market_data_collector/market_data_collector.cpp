#include "market_data_collector.h"
#include "../hotspine/hotspine_writer.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <ctime>
#include <iomanip>
#include <sstream>

// Helper function for timestamped logging
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::tm tm = *std::localtime(&now_time);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    
    char ms_buffer[10];
    snprintf(ms_buffer, sizeof(ms_buffer), "%03d", static_cast<int>(now_ms.count()));
    
    return std::string(buffer) + "." + ms_buffer;
}

MarketDataCollector::MarketDataCollector(const Config& cfg)
    : config_(cfg) {

    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Initializing with configuration..." << std::endl;
    
    // Log configuration details
    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Configuration details:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   MS SQL enabled: " << (config_.enable_mssql ? "true" : "false") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Exclusive HotSpine: " << (config_.enable_exclusive_hotspine ? "true" : "false") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Timeframes: ";
    for (const auto& tf : config_.timeframes) {
        std::cout << tf << " ";
    }
    std::cout << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Exchanges: " << config_.exchanges.size() << std::endl;
    for (const auto& ex : config_.exchanges) {
        std::cout << "[" << getCurrentTimestamp() << "][INFO]     - " << ex.exchange_name
                  << " (symbols: ";
        for (const auto& sym : ex.symbols) {
            std::cout << sym << " ";
        }
        std::cout << ", channels: ";
        for (const auto& ch : ex.channels) {
            std::cout << ch << " ";
        }
        std::cout << ", market_type: " << ex.market_type << ")" << std::endl;
    }
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Buffer sizes - Trades: " << config_.trade_buffer_size
              << ", Candles: " << config_.candle_buffer_size
              << ", Orderbooks: " << config_.orderbook_buffer_size << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Intervals - Flush: " << config_.flush_interval_ms
              << "ms, Stats: " << config_.stats_report_interval_s << "s" << std::endl;

    // Conditionally initialize MS SQL database
    if (config_.enable_mssql) {
        std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Initializing MS SQL database connection" << std::endl;
        db_ = std::make_shared<MSSQLBulkInserter>(
            config_.db_connection_string);
        
        // Enable debug mode if configured
        if (config_.debug_config.enabled && config_.debug_config.database_debug) {
            std::cout << "[" << getCurrentTimestamp() << "][DEBUG] MarketDataCollector: Enabling database debug mode" << std::endl;
            db_->setDebugMode(true);
            db_->enableDetailedLogging(config_.debug_config.verbose_logging);
        }
        
        std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: MS SQL database connection initialized" << std::endl;
    } else {
        std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: MS SQL database disabled" << std::endl;
    }

    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Initializing candle aggregator with timeframes" << std::endl;
    candle_agg_ = std::make_shared<CandleAggregator>(
        config_.timeframes);
    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Candle aggregator initialized" << std::endl;

    // Create HotSpine writer with optimized settings for low latency
    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Initializing HotSpine writer" << std::endl;
    hotspine_writer_ = std::make_shared<HotSpine::HotSpineWriter>("/btquant_hotspine");
    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: HotSpine writer initialized" << std::endl;
    hotspine_writer_->setBatchingEnabled(true);
    hotspine_writer_->setBatchSize(50); // Smaller batch size for lower latency
    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: HotSpine batching configured (size: 50)" << std::endl;

    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Initializing market data processor" << std::endl;
    processor_ = std::make_shared<MarketDataProcessor>(db_, candle_agg_, hotspine_writer_, config_.enable_exclusive_hotspine);
    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Market data processor initialized" << std::endl;
     
    // Log HotSpine integration status
    if (hotspine_writer_) {
        std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: HotSpine integration ENABLED" << std::endl;
        if (config_.enable_exclusive_hotspine) {
            std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Running in EXCLUSIVE HotSpine mode (database disabled)" << std::endl;
        } else {
            std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Running in DUAL mode (HotSpine + database)" << std::endl;
        }
    } else {
        std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: HotSpine integration DISABLED" << std::endl;
    }

    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Setting buffer limits" << std::endl;
    processor_->setBufferLimits(config_.trade_buffer_size,
                                config_.candle_buffer_size,
                                config_.orderbook_buffer_size);
    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Buffer limits set successfully" << std::endl;

    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Initializing exchange connection manager" << std::endl;
    conn_mgr_ = std::make_unique<ExchangeConnectionManager>(processor_);
    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Exchange connection manager initialized" << std::endl;

    std::cout << "[" << getCurrentTimestamp() << "][INFO] MarketDataCollector: Initialization completed successfully" << std::endl;
}

MarketDataCollector::~MarketDataCollector() {
    stop();
}

void MarketDataCollector::start() {
    running_ = true;
    
    // Add comprehensive WebSocket debugging
    conn_mgr_->addWebSocketDebugging();
    
    // Log WebSocket debug information
    conn_mgr_->logWebSocketDebugInfo();
    
    // Log WebSocket status before starting
    conn_mgr_->logWebSocketStatus();
    
    // Check WebSocket connection health
    conn_mgr_->checkWebSocketConnection();
    
    // Monitor WebSocket data flow
    conn_mgr_->monitorWebSocketDataFlow();
    
    conn_mgr_->subscribe(config_.exchanges);
    conn_mgr_->start();
     
    // Add WebSocket debugging
    processor_->addWebSocketDebugging();
    
    // Run comprehensive WebSocket diagnostics
    conn_mgr_->diagnoseWebSocketIssues();
    
    // Log session status after starting
    conn_mgr_->logSessionStatus();
    
    // Check WebSocket connection health after starting
    conn_mgr_->checkWebSocketConnection();
    
    // Monitor WebSocket data flow after starting
    conn_mgr_->monitorWebSocketDataFlow();
    
    flush_thread_ = std::thread(&MarketDataCollector::flushLoop, this);
    stats_thread_ = std::thread(&MarketDataCollector::statsLoop, this);
}

void MarketDataCollector::stop() {
    if (!running_) return;
    running_ = false;
    conn_mgr_->stop();

    if (flush_thread_.joinable()) flush_thread_.join();
    if (stats_thread_.joinable()) stats_thread_.join();

    // final flush
    processor_->flushBuffers();
    
    // Final HotSpine flush
    if (hotspine_writer_) {
        hotspine_writer_->flushBatch();
    }
    
    auto final_candles = candle_agg_->flushAll();
    if (!final_candles.empty() && config_.enable_mssql) {
        // group per table
        std::unordered_map<std::string,
                           std::vector<MarketData::OHLCV>> per_table;
        for (const auto& c : final_candles) {
            per_table[c.getTableName()].push_back(c);
        }
        for (auto& kv : per_table) {
            db_->bulkInsertOHLCV(kv.first, kv.second);
        }
    }
}

void MarketDataCollector::waitForShutdown() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void MarketDataCollector::printStats() const {
    auto st = processor_->getStats();
    std::cout << "=== Market Data Stats ===\n";
    std::cout << "Trades: received=" << st.trades_received
              << ", inserted=" << st.trades_inserted
              << ", rate=" << st.trades_per_sec << " /s\n";
    std::cout << "Candles: generated=" << st.candles_generated
              << ", inserted=" << st.candles_inserted << "\n";
    std::cout << "Orderbooks: received=" << st.orderbooks_received
              << ", inserted=" << st.orderbooks_inserted
              << ", rate=" << st.orderbooks_per_sec << " /s\n";
    std::cout << "Avg latency (ms): " << st.avg_latency_ms << "\n";
    std::cout << "Errors: " << st.errors << "\n";

    // scrape-friendly JSON line for Prom/Grafana → Loki/Tempo/etc.
    std::cout << "STATS_JSON " << processor_->getStatsJson() << "\n";
}

void MarketDataCollector::flushLoop() {
    while (running_) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.flush_interval_ms));
        processor_->flushBuffers();
        
        // Also flush HotSpine batch if writer is available
        if (hotspine_writer_) {
            hotspine_writer_->flushBatch();
        }
    }
}

void MarketDataCollector::statsLoop() const {
    int health_check_counter = 0;
    int data_flow_counter = 0;
    int candle_validation_counter = 0;
    while (const_cast<std::atomic<bool>&>(running_)) {
        std::this_thread::sleep_for(
            std::chrono::seconds(config_.stats_report_interval_s));
        if (!const_cast<std::atomic<bool>&>(running_)) break;
        
        printStats();
        
        // Log HotSpine statistics if enabled
        if (hotspine_writer_) {
            std::string hotspine_stats = hotspine_writer_->getDetailedStats();
            std::cout << "[" << getCurrentTimestamp() << "][INFO] HotSpine Stats: " << hotspine_stats << std::endl;
        }
        
        // Log WebSocket data flow statistics every 3 stats intervals
        data_flow_counter++;
        if (data_flow_counter >= 3) {
            data_flow_counter = 0;
            const_cast<MarketDataProcessor*>(processor_.get())->logWebSocketDataFlowStats();
        }
        
        // Validate WebSocket data flow every 10 stats intervals
        static int validation_counter = 0;
        validation_counter++;
        if (validation_counter >= 10) {
            validation_counter = 0;
            const_cast<MarketDataProcessor*>(processor_.get())->validateWebSocketDataFlow();
        }
        
        // Validate candle aggregation every 7 stats intervals
        candle_validation_counter++;
        if (candle_validation_counter >= 7) {
            candle_validation_counter = 0;
            const_cast<CandleAggregator*>(candle_agg_.get())->validateCandleAggregation();
        }
        
        // Perform WebSocket health check every 5 stats intervals
        health_check_counter++;
        if (health_check_counter >= 5) {
            health_check_counter = 0;
            const_cast<ExchangeConnectionManager*>(conn_mgr_.get())->checkWebSocketConnection();
        }
    }
}
