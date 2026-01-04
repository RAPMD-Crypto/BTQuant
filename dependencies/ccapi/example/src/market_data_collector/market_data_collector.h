#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include "candle_aggregator.h"
#include "exchange_connection_manager.h"
#include "market_data_processor.h"
#include "mssql_bulk_inserter.h"

class MarketDataCollector {
public:
    struct Config {
        std::string db_connection_string;
        std::size_t bulk_insert_batch_size{500};

        std::vector<ExchangeConnectionManager::ExchangeConfig> exchanges;

        std::vector<std::string> timeframes{"1m", "5m", "15m", "1h"};

        std::size_t trade_buffer_size{1000};
        std::size_t candle_buffer_size{200};
        std::size_t orderbook_buffer_size{100};

        int flush_interval_ms{1000};
        int stats_report_interval_s{10};

        // New configuration options
        bool enable_mssql{true};           // Enable/disable MS SQL database
        bool enable_exclusive_hotspine{false}; // Enable exclusive hotswap mode
        
        // Debug mode configuration
        struct DebugConfig {
            bool enabled{false};
            bool verbose_logging{false};
            bool websocket_debug{false};
            bool database_debug{false};
            bool buffer_debug{false};
            bool flush_debug{false};
            bool performance_monitoring{false};
            bool connection_health_checks{false};
            bool data_flow_validation{false};
            bool error_tracing{false};
            std::string timestamp_precision{"milliseconds"};
            std::string log_level{"INFO"};
            
            // Database debug settings
            struct DatabaseDebug {
                bool verify_connection_on_start{false};
                bool test_table_creation{false};
                bool log_all_queries{false};
                bool log_connection_status{false};
                bool enable_detailed_error_logging{false};
                int connection_timeout_ms{30000};
                int command_timeout_ms{60000};
            } database_debug_config;
            
            // WebSocket debug settings
            struct WebSocketDebug {
                bool log_connection_attempts{false};
                bool log_subscription_status{false};
                bool log_data_reception{false};
                bool ping_pong_monitoring{false};
                bool connection_health_monitoring{false};
                bool timeout_debugging{false};
                bool protocol_error_logging{false};
                bool data_rate_monitoring{false};
            } websocket_debug_config;
            
            // Buffer debug settings
            struct BufferDebug {
                bool log_buffer_operations{false};
                bool log_threshold_crossing{false};
                bool log_flush_decisions{false};
                bool monitor_memory_usage{false};
                bool track_buffer_sizes{false};
                bool log_batch_operations{false};
            } buffer_debug_config;
            
            // Performance monitoring settings
            struct PerformanceMonitoring {
                bool measure_processing_latency{false};
                bool track_throughput{false};
                bool monitor_resource_usage{false};
                bool log_performance_metrics{false};
                bool alert_on_slow_operations{false};
            } performance_config;
        } debug_config;
    };

    explicit MarketDataCollector(const Config& cfg);
    ~MarketDataCollector();

    void start();
    void stop();
    void waitForShutdown();

    void printStats() const;

private:
    Config config_;
    std::shared_ptr<MSSQLBulkInserter> db_;
    std::shared_ptr<CandleAggregator> candle_agg_;
    std::shared_ptr<HotSpine::HotSpineWriter> hotspine_writer_;
    std::shared_ptr<MarketDataProcessor> processor_;
    std::unique_ptr<ExchangeConnectionManager> conn_mgr_;

    std::thread flush_thread_;
    std::thread stats_thread_;
    std::atomic<bool> running_{false};

    void flushLoop();
    void statsLoop() const;
};
