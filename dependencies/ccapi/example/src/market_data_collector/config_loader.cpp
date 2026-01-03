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

    std::cout << "[" << getCurrentTimestamp() << "][INFO] ConfigLoader: Configuration loading completed successfully" << std::endl;

    return cfg;
}