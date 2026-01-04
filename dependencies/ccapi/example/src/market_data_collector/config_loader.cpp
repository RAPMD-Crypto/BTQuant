#include <fstream>
#include <iostream>
#include "market_data_collector.h"
#include "nlohmann/json.hpp"
#include "utilities.h"

using json = nlohmann::json;

// Helper functions from utilities.h

std::string createConnectionString(const std::string& server,
                                    const std::string& database,
                                    const std::string& user,
                                    const std::string& pwd) {
    return
        "DRIVER={ODBC Driver 18 for SQL Server};"
        "SERVER=" + server + ";"
        "DATABASE=" + database + ";"
        "UID=" + user + ";"
        "PWD=" + pwd + ";"
        "TrustServerCertificate=yes;"
        "MARS_Connection=yes;"
        "Connection Timeout=30;"
        "Command Timeout=60;";
}

MarketDataCollector::Config loadConfig(const std::string& path) {
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Starting configuration loading from " << path << std::endl;
    
    std::ifstream in(path);
    json j;

    if (!in) {
        // If config file doesnt exist, use default values
        std::cout << "[" << getCurrentTimestamp() << "][WARNING] ConfigLoader: Config file not found: " << path << ", using default configuration." << std::endl;
        
        // Create default configuration
        MarketDataCollector::Config cfg;
        
        // Default database connection (will be used if enable_mssql is true)
        cfg.db_connection_string = createConnectionString(
            "localhost",
            "market_data",
            "sa",
            "your_password");
        
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Default database connection string created" << std::endl;
        
        // Default timeframes
        cfg.timeframes = {"1m", "5m", "15m", "1h"};
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Default timeframes set: 1m, 5m, 15m, 1h" << std::endl;
        
        // Default exchange configuration
        ExchangeConnectionManager::ExchangeConfig ec;
        ec.exchange_name = "binance";
        ec.symbols = {"BTC-USDT", "ETH-USDT"};
        ec.channels = {"TRADE", "MARKET_DEPTH"};
        ec.market_type = "spot";
        cfg.exchanges.push_back(std::move(ec));
        
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Default exchange configuration added: binance with BTC-USDT, ETH-USDT" << std::endl;
        
        // Default buffer sizes and intervals
        cfg.trade_buffer_size = 1000;
        cfg.candle_buffer_size = 200;
        cfg.orderbook_buffer_size = 100;
        cfg.flush_interval_ms = 1000;
        cfg.stats_report_interval_s = 10;
        
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Default buffer sizes set:" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Trade buffer: " << cfg.trade_buffer_size << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Candle buffer: " << cfg.candle_buffer_size << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Orderbook buffer: " << cfg.orderbook_buffer_size << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Flush interval: " << cfg.flush_interval_ms << "ms" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Stats interval: " << cfg.stats_report_interval_s << "s" << std::endl;
        
        // Default toggle settings - MS SQL enabled, hotswap disabled
        cfg.enable_mssql = true;
        cfg.enable_exclusive_hotspine = false;
        
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Default toggle settings:" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   MS SQL enabled: " << (cfg.enable_mssql ? "true" : "false") << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Exclusive HotSpine: " << (cfg.enable_exclusive_hotspine ? "true" : "false") << std::endl;
        
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Default configuration loading completed" << std::endl;
        
        return cfg;
    }
    
    // Load configuration from file
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Loading JSON configuration from file" << std::endl;
    in >> j;

    MarketDataCollector::Config cfg;

    // Load database configuration
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Loading database configuration" << std::endl;
    auto db = j.at("db");
    std::string server = db.at("server").get<std::string>();
    std::string database = db.at("database").get<std::string>();
    std::string user = db.at("user").get<std::string>();
    std::string password = db.at("password").get<std::string>();
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Database connection details:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Server: " << server << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Database: " << database << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   User: " << user << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Password: [REDACTED]" << std::endl;
    
    cfg.db_connection_string = createConnectionString(server, database, user, password);
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Database connection string created" << std::endl;

    // Load timeframes
    cfg.timeframes = j.at("timeframes").get<std::vector<std::string>>();
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Timeframes loaded: ";
    for (const auto& tf : cfg.timeframes) {
        std::cout << tf << " ";
    }
    std::cout << std::endl;

    // Load exchange configurations
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Loading exchange configurations" << std::endl;
    for (const auto& ex : j.at("exchanges")) {
        ExchangeConnectionManager::ExchangeConfig ec;

        ec.exchange_name = ex.at("name").get<std::string>();
        ec.symbols       = ex.at("symbols")
                             .get<std::vector<std::string>>();
        ec.channels      = ex.value(
            "channels",
            std::vector<std::string>{"TRADE", "MARKET_DEPTH"}
        );
        ec.market_type   = ex.value(
            "market_type",
            std::string("spot")
        );

        std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Exchange configuration loaded:" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Exchange: " << ec.exchange_name << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Symbols: ";
        for (const auto& sym : ec.symbols) {
            std::cout << sym << " ";
        }
        std::cout << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Channels: ";
        for (const auto& ch : ec.channels) {
            std::cout << ch << " ";
        }
        std::cout << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Market type: " << ec.market_type << std::endl;

        cfg.exchanges.push_back(std::move(ec));
    }

    // Load buffer sizes and intervals
    cfg.trade_buffer_size        = j.value("trade_buffer_size", 500);
    cfg.candle_buffer_size       = j.value("candle_buffer_size", 200);
    cfg.orderbook_buffer_size    = j.value("orderbook_buffer_size", 100);
    cfg.flush_interval_ms        = j.value("flush_interval_ms", 1000);
    cfg.stats_report_interval_s  = j.value("stats_report_interval_s", 10);

    std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Buffer and interval settings loaded:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Trade buffer size: " << cfg.trade_buffer_size << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Candle buffer size: " << cfg.candle_buffer_size << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Orderbook buffer size: " << cfg.orderbook_buffer_size << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Flush interval: " << cfg.flush_interval_ms << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Stats report interval: " << cfg.stats_report_interval_s << "s" << std::endl;

    // Load new configuration options
    cfg.enable_mssql              = j.value("enable_mssql", true);
    cfg.enable_exclusive_hotspine = j.value("enable_exclusive_hotspine", false);

    std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Feature toggle settings loaded:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Enable MS SQL: " << (cfg.enable_mssql ? "true" : "false") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Enable exclusive HotSpine: " << (cfg.enable_exclusive_hotspine ? "true" : "false") << std::endl;

    // Load debug mode configuration
    if (j.contains("debug_mode")) {
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Loading debug mode configuration" << std::endl;
        auto debug = j.at("debug_mode");
        
        cfg.debug_config.enabled                    = debug.value("enabled", false);
        cfg.debug_config.verbose_logging            = debug.value("verbose_logging", false);
        cfg.debug_config.websocket_debug            = debug.value("websocket_debug", false);
        // Handle database_debug: if boolean, use it; if object or present, enable
        if (debug.contains("database_debug") && debug["database_debug"].is_boolean()) {
            cfg.debug_config.database_debug = debug["database_debug"].get<bool>();
        } else {
            cfg.debug_config.database_debug = debug.contains("database_debug");
        }
        cfg.debug_config.buffer_debug               = debug.value("buffer_debug", false);
        cfg.debug_config.flush_debug                = debug.value("flush_debug", false);
        cfg.debug_config.performance_monitoring     = debug.value("performance_monitoring", false);
        cfg.debug_config.connection_health_checks   = debug.value("connection_health_checks", false);
        cfg.debug_config.data_flow_validation       = debug.value("data_flow_validation", false);
        cfg.debug_config.error_tracing              = debug.value("error_tracing", false);
        cfg.debug_config.timestamp_precision        = debug.value("timestamp_precision", std::string("milliseconds"));
        cfg.debug_config.log_level                  = debug.value("log_level", std::string("INFO"));
        
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Debug mode settings loaded:" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Debug enabled: " << (cfg.debug_config.enabled ? "true" : "false") << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Verbose logging: " << (cfg.debug_config.verbose_logging ? "true" : "false") << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket debug: " << (cfg.debug_config.websocket_debug ? "true" : "false") << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Database debug: " << (cfg.debug_config.database_debug ? "true" : "false") << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Buffer debug: " << (cfg.debug_config.buffer_debug ? "true" : "false") << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Log level: " << cfg.debug_config.log_level << std::endl;
        
        // Load database debug configuration
        if (debug.contains("database_debug") && debug["database_debug"].is_object()) {
            auto dbg = debug.at("database_debug");
            cfg.debug_config.database_debug_config.verify_connection_on_start = dbg.value("verify_connection_on_start", false);
            cfg.debug_config.database_debug_config.test_table_creation        = dbg.value("test_table_creation", false);
            cfg.debug_config.database_debug_config.log_all_queries            = dbg.value("log_all_queries", false);
            cfg.debug_config.database_debug_config.log_connection_status      = dbg.value("log_connection_status", false);
            cfg.debug_config.database_debug_config.enable_detailed_error_logging = dbg.value("enable_detailed_error_logging", false);
            cfg.debug_config.database_debug_config.connection_timeout_ms      = dbg.value("connection_timeout_ms", 30000);
            cfg.debug_config.database_debug_config.command_timeout_ms         = dbg.value("command_timeout_ms", 60000);
        }
        
        // Load WebSocket debug configuration
        if (debug.contains("websocket_debug") && debug["websocket_debug"].is_object()) {
            auto wsd = debug.at("websocket_debug");
            cfg.debug_config.websocket_debug_config.log_connection_attempts     = wsd.value("log_connection_attempts", false);
            cfg.debug_config.websocket_debug_config.log_subscription_status     = wsd.value("log_subscription_status", false);
            cfg.debug_config.websocket_debug_config.log_data_reception          = wsd.value("log_data_reception", false);
            cfg.debug_config.websocket_debug_config.ping_pong_monitoring        = wsd.value("ping_pong_monitoring", false);
            cfg.debug_config.websocket_debug_config.connection_health_monitoring = wsd.value("connection_health_monitoring", false);
            cfg.debug_config.websocket_debug_config.timeout_debugging            = wsd.value("timeout_debugging", false);
            cfg.debug_config.websocket_debug_config.protocol_error_logging       = wsd.value("protocol_error_logging", false);
            cfg.debug_config.websocket_debug_config.data_rate_monitoring         = wsd.value("data_rate_monitoring", false);
        }
        
        // Load buffer debug configuration
        if (debug.contains("buffer_debug") && debug["buffer_debug"].is_object()) {
            auto bd = debug.at("buffer_debug");
            cfg.debug_config.buffer_debug_config.log_buffer_operations  = bd.value("log_buffer_operations", false);
            cfg.debug_config.buffer_debug_config.log_threshold_crossing = bd.value("log_threshold_crossing", false);
            cfg.debug_config.buffer_debug_config.log_flush_decisions    = bd.value("log_flush_decisions", false);
            cfg.debug_config.buffer_debug_config.monitor_memory_usage   = bd.value("monitor_memory_usage", false);
            cfg.debug_config.buffer_debug_config.track_buffer_sizes     = bd.value("track_buffer_sizes", false);
            cfg.debug_config.buffer_debug_config.log_batch_operations   = bd.value("log_batch_operations", false);
        }
        
        // Load performance monitoring configuration
        if (debug.contains("performance_monitoring") && debug["performance_monitoring"].is_object()) {
            auto pm = debug.at("performance_monitoring");
            cfg.debug_config.performance_config.measure_processing_latency = pm.value("measure_processing_latency", false);
            cfg.debug_config.performance_config.track_throughput           = pm.value("track_throughput", false);
            cfg.debug_config.performance_config.monitor_resource_usage     = pm.value("monitor_resource_usage", false);
            cfg.debug_config.performance_config.log_performance_metrics    = pm.value("log_performance_metrics", false);
            cfg.debug_config.performance_config.alert_on_slow_operations   = pm.value("alert_on_slow_operations", false);
        }
        
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Debug mode configuration completed successfully" << std::endl;
    } else {
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: No debug mode configuration found, using defaults" << std::endl;
    }

    std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Configuration loading completed successfully" << std::endl;

    return cfg;
}