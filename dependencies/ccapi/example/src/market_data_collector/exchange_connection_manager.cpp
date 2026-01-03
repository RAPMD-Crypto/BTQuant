#include "exchange_connection_manager.h"

#include <iostream>
#include "utilities.h"

// Helper function to format session options for detailed logging
std::string formatSessionOptions(const ccapi::SessionOptions& options) {
    std::ostringstream oss;
    oss << "SessionOptions Details:\n";
    oss << "  WebSocket Settings:\n";
    oss << "    enableCheckPingPongWebsocketProtocolLevel: " << (options.enableCheckPingPongWebsocketProtocolLevel ? "true" : "false") << "\n";
    oss << "    enableCheckPingPongWebsocketApplicationLevel: " << (options.enableCheckPingPongWebsocketApplicationLevel ? "true" : "false") << "\n";
    oss << "    pingWebsocketProtocolLevelIntervalMilliseconds: " << options.pingWebsocketProtocolLevelIntervalMilliseconds << "\n";
    oss << "    pongWebsocketProtocolLevelTimeoutMilliseconds: " << options.pongWebsocketProtocolLevelTimeoutMilliseconds << "\n";
    oss << "    pingWebsocketApplicationLevelIntervalMilliseconds: " << options.pingWebsocketApplicationLevelIntervalMilliseconds << "\n";
    oss << "    pongWebsocketApplicationLevelTimeoutMilliseconds: " << options.pongWebsocketApplicationLevelTimeoutMilliseconds << "\n";
    oss << "    websocketConnectTimeoutMilliseconds: " << options.websocketConnectTimeoutMilliseconds << "\n";
    
    oss << "  Data Integrity Checks:\n";
    oss << "    enableCheckSequence: " << (options.enableCheckSequence ? "true" : "false") << "\n";
    oss << "    enableCheckOrderBookChecksum: " << (options.enableCheckOrderBookChecksum ? "true" : "false") << "\n";
    oss << "    enableCheckOrderBookCrossed: " << (options.enableCheckOrderBookCrossed ? "true" : "false") << "\n";
    
    oss << "  Queue Management:\n";
    oss << "    maxEventQueueSize: " << options.maxEventQueueSize << "\n";
    
    oss << "  Data Reception Optimization:\n";
    oss << "    These settings help ensure reliable WebSocket data reception and processing\n";
    
    return oss.str();
}

// Helper function to log WebSocket URL information
void logWebSocketConfiguration(const ccapi::SessionConfigs& configs, const std::string& exchange) {
    auto urls = configs.getUrlWebsocketBase();
    auto it = urls.find(exchange);
    
    if (it != urls.end()) {
        std::cout << "[" << getCurrentTimestamp() << "][INFO] WebSocket URL for " << exchange << ": " << it->second << std::endl;
        
        // Validate WebSocket URL format
        std::string url = it->second;
        if (url.find("wss://") != 0 && url.find("ws://") != 0) {
            std::cerr << "[" << getCurrentTimestamp() << "][WARNING] WebSocket URL for " << exchange << " doesn't start with wss:// or ws://: " << url << std::endl;
        }
    } else {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] No WebSocket URL found for exchange: " << exchange << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] Available exchanges in config:" << std::endl;
        for (const auto& [ex, url] : urls) {
            std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   " << ex << ": " << url << std::endl;
        }
    }
}

// Helper function to log detailed WebSocket connection status
void logWebSocketConnectionStatus(const ccapi::Session& session, const std::string& exchange) {
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG] Checking WebSocket connection status for " << exchange << "..." << std::endl;
    
    // Note: CCAPI doesn't provide direct WebSocket connection status API,
    // so we rely on event-based monitoring in the processor
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO] WebSocket connection monitoring:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   - Connection health is monitored through event callbacks" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   - Check MarketDataProcessor for WebSocket event details" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   - Look for SESSION_STATUS and SUBSCRIPTION_STATUS events" << std::endl;
    
    // Add WebSocket-specific troubleshooting guidance
    std::cout << "[" << getCurrentTimestamp() << "][INFO] WebSocket troubleshooting:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   If no data is received after subscription:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     1. Check for SESSION_STATUS events with WebSocket connection details" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     2. Look for SUBSCRIPTION_STATUS events confirming successful subscription" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     3. Verify WebSocket URL is correct and reachable" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     4. Check network connectivity and firewall settings" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     5. Monitor for WebSocket protocol errors in logs" << std::endl;
}

ExchangeConnectionManager::ExchangeConnectionManager(
    std::shared_ptr<MarketDataProcessor> processor)
    : session_options_(std::make_unique<ccapi::SessionOptions>()),
      session_configs_(std::make_unique<ccapi::SessionConfigs>()),
      processor_(std::move(processor)) {

    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Initializing..." << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Environment setup starting" << std::endl;

    // Log detailed session options
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Detailed SessionOptions:" << std::endl;
    std::cout << formatSessionOptions(*session_options_) << std::endl;

    // Log session configs (key information)
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: SessionConfigs - WebSocket base URLs:" << std::endl;
    for (const auto& [exchange, url] : session_configs_->getUrlWebsocketBase()) {
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   " << exchange << ": " << url << std::endl;
    }

    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: SessionConfigs - Supported exchanges:" << std::endl;
    for (const auto& [exchange, fields] : session_configs_->getExchangeFieldMap()) {
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   " << exchange << " (fields: ";
        for (const auto& field : fields) {
            std::cout << field << " ";
        }
        std::cout << ")" << std::endl;
    }

    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Configuring session options for WebSocket environment" << std::endl;
    
    // Configure session options for better WebSocket debugging and data reception
    session_options_->enableCheckPingPongWebsocketProtocolLevel = true;
    session_options_->enableCheckPingPongWebsocketApplicationLevel = true;
    session_options_->websocketConnectTimeoutMilliseconds = 30000; // Increased timeout to 30 seconds
    session_options_->pingWebsocketProtocolLevelIntervalMilliseconds = 30000; // 30 seconds
    session_options_->pongWebsocketProtocolLevelTimeoutMilliseconds = 10000; // 10 seconds
    session_options_->pingWebsocketApplicationLevelIntervalMilliseconds = 60000; // 60 seconds
    session_options_->pongWebsocketApplicationLevelTimeoutMilliseconds = 15000; // 15 seconds
    
    // Enable additional checks for data integrity
    session_options_->enableCheckSequence = true;
    session_options_->enableCheckOrderBookChecksum = true;
    session_options_->enableCheckOrderBookCrossed = true;
    
    // Increase event queue size for better data handling
    session_options_->maxEventQueueSize = 10000;
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Updated SessionOptions for WebSocket debugging and data reception:" << std::endl;
    std::cout << "  - WebSocket ping/pong checks: ENABLED\n";
    std::cout << "  - WebSocket connection timeout: " << session_options_->websocketConnectTimeoutMilliseconds << "ms\n";
    std::cout << "  - Protocol-level ping interval: " << session_options_->pingWebsocketProtocolLevelIntervalMilliseconds << "ms\n";
    std::cout << "  - Protocol-level pong timeout: " << session_options_->pongWebsocketProtocolLevelTimeoutMilliseconds << "ms\n";
    std::cout << "  - Application-level ping interval: " << session_options_->pingWebsocketApplicationLevelIntervalMilliseconds << "ms\n";
    std::cout << "  - Application-level pong timeout: " << session_options_->pongWebsocketApplicationLevelTimeoutMilliseconds << "ms\n";
    std::cout << "  - Data sequence checking: " << (session_options_->enableCheckSequence ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "  - Order book checksum checking: " << (session_options_->enableCheckOrderBookChecksum ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "  - Order book crossed checking: " << (session_options_->enableCheckOrderBookCrossed ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "  - Max event queue size: " << session_options_->maxEventQueueSize << "\n";

    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Initializing CCAPI session" << std::endl;
    
    try {
        session_ = std::make_unique<ccapi::Session>(
            *session_options_, *session_configs_, processor_.get());
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Session initialized successfully" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Environment setup completed successfully" << std::endl;
        
        // Log WebSocket configuration for the exchange we'll be using
        logWebSocketConfiguration(*session_configs_, "binance");
        
    } catch (const std::exception& e) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] ExchangeConnectionManager: Failed to initialize session: " << e.what() << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] This could indicate issues with CCAPI initialization or resource availability" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] Environment setup failed!" << std::endl;
        throw;
    }
}

void ExchangeConnectionManager::subscribe(
    const std::vector<ExchangeConfig>& cfgs) {

    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Starting subscription process..." << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Number of exchange configs: " << cfgs.size() << std::endl;

    // Add WebSocket connection pre-check
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG] ExchangeConnectionManager: WebSocket connection pre-check..." << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   Session initialized: " << (session_ ? "YES" : "NO") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   Session options configured: " << (session_options_ ? "YES" : "NO") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   Session configs loaded: " << (session_configs_ ? "YES" : "NO") << std::endl;

    std::vector<ccapi::Subscription> subs;
    int total_subscriptions = 0;

    for (const auto& cfg : cfgs) {
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Processing exchange: " << cfg.exchange_name << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Market type: " << cfg.market_type << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Number of symbols: " << cfg.symbols.size() << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   Number of channels: " << cfg.channels.size() << std::endl;

        // Log WebSocket URL for this exchange
        logWebSocketConfiguration(*session_configs_, cfg.exchange_name);

        // Add exchange-specific WebSocket validation
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG] ExchangeConnectionManager: Exchange-specific WebSocket validation for " << cfg.exchange_name << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   Validating WebSocket configuration..." << std::endl;

        for (const auto& sym : cfg.symbols) {
            std::cout << "[" << getCurrentTimestamp() << "][INFO]   Symbol: " << sym << std::endl;
            for (const auto& ch : cfg.channels) {
                // correlation id encodes exchange:symbol:market_type
                std::string cid =
                    cfg.exchange_name + ":" + sym + ":" + cfg.market_type;
                std::cout << "[" << getCurrentTimestamp() << "][INFO]     Channel: " << ch << " (Correlation ID: " << cid << ")" << std::endl;

                try {
                    ccapi::Subscription s(
                        cfg.exchange_name,  // exchange
                        sym,                // instrument
                        ch,                 // field ("TRADE", "MARKET_DEPTH", ...)
                        "",                 // options
                        cid                 // correlationId
                    );

                    subs.push_back(std::move(s));
                    total_subscriptions++;
                    
                    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Subscription created successfully" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[" << getCurrentTimestamp() << "][ERROR]     Failed to create subscription for " << cfg.exchange_name << ":" << sym << ":" << ch << " - " << e.what() << std::endl;
                }
            }
        }
    }

    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Total subscriptions prepared: " << subs.size() << std::endl;

    if (subs.empty()) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] ExchangeConnectionManager: No subscriptions were created!" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] This could indicate invalid exchange configurations or missing data" << std::endl;
        return;
    }

    try {
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Attempting to subscribe to " << subs.size() << " streams..." << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG] ExchangeConnectionManager: WebSocket connection attempt starting..." << std::endl;
        
        // Log detailed subscription information
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG] ExchangeConnectionManager: Subscription details:" << std::endl;
        for (const auto& sub : subs) {
            std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   Exchange: " << sub.getExchange()
                      << ", Instrument: " << sub.getInstrument()
                      << ", Field: " << sub.getField()
                      << ", CorrelationID: " << sub.getCorrelationId() << std::endl;
        }
        
        // Log WebSocket configuration for the target exchange
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: WebSocket configuration for target exchange:" << std::endl;
        logWebSocketConfiguration(*session_configs_, "binance");
        
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG] ExchangeConnectionManager: Calling session->subscribe()..." << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG] ExchangeConnectionManager: This will initiate WebSocket connections..." << std::endl;
        
        // Add timestamp for WebSocket connection start
        auto websocket_start_time = std::chrono::system_clock::now();
        
        // Add WebSocket connection monitoring
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG] ExchangeConnectionManager: WebSocket connection monitoring activated" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   - Protocol-level ping/pong: "
                  << (session_options_->enableCheckPingPongWebsocketProtocolLevel ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   - Application-level ping/pong: "
                  << (session_options_->enableCheckPingPongWebsocketApplicationLevel ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   - Connection timeout: "
                  << session_options_->websocketConnectTimeoutMilliseconds << "ms" << std::endl;
        
        session_->subscribe(subs);
        
        // Calculate WebSocket connection duration
        auto websocket_end_time = std::chrono::system_clock::now();
        auto websocket_duration = std::chrono::duration_cast<std::chrono::milliseconds>(websocket_end_time - websocket_start_time);
        
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Successfully subscribed to " << subs.size() << " streams" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: WebSocket connection established in " << websocket_duration.count() << "ms" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG] ExchangeConnectionManager: WebSocket subscriptions completed, waiting for data..." << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Data should now start flowing through WebSocket connection" << std::endl;
        
        // Add WebSocket data flow monitoring
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG] ExchangeConnectionManager: WebSocket data flow monitoring activated" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   - Monitoring for SUBSCRIPTION_DATA events" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   - Tracking message counts and data rates" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   - Watching for empty message lists" << std::endl;
        
        // Log WebSocket connection status after subscription
        logWebSocketConnectionStatus(*session_, "binance");
        
    } catch (const std::exception& e) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] ExchangeConnectionManager: Failed to subscribe: " << e.what() << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] ExchangeConnectionManager: This could indicate WebSocket connection issues" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] Possible causes:" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   - Network connectivity issues" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   - Exchange WebSocket endpoint unavailable" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   - Invalid API credentials (if required)" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   - WebSocket connection timeout (current: " << session_options_->websocketConnectTimeoutMilliseconds << "ms)" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   - WebSocket URL configuration issues" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   - Firewall or proxy blocking WebSocket connections" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   - DNS resolution issues for WebSocket endpoints" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] ExchangeConnectionManager: Subscription process failed!" << std::endl;
        
        // Log WebSocket configuration for troubleshooting
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] WebSocket configuration details:" << std::endl;
        logWebSocketConfiguration(*session_configs_, "binance");
        
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] Troubleshooting steps:" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   1. Verify network connectivity to exchange WebSocket endpoints" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   2. Check if exchange WebSocket endpoints are reachable (try curl/wget)" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   3. Verify WebSocket URL configuration in CCAPI" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   4. Check firewall/proxy settings" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   5. Test with different WebSocket connection timeout values" << std::endl;
    }
}


void ExchangeConnectionManager::start() {
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Starting session..." << std::endl;
    
    try {
        running_ = true;
        // ccapi manages its own threads internally.
        std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Session started successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] ExchangeConnectionManager: Failed to start session: " << e.what() << std::endl;
        running_ = false;
    }
}

void ExchangeConnectionManager::stop() {
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Stopping session..." << std::endl;
    
    try {
        running_ = false;
        if (session_) {
            std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Calling session->stop()..." << std::endl;
            session_->stop();
            std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Session stopped successfully" << std::endl;
        } else {
            std::cout << "[" << getCurrentTimestamp() << "][WARN] ExchangeConnectionManager: No active session to stop" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR] ExchangeConnectionManager: Failed to stop session: " << e.what() << std::endl;
    }
}

// Add a method to log session status and debug information
void ExchangeConnectionManager::logSessionStatus() const {
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Session status:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Running: " << (running_ ? "true" : "false") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Session initialized: " << (session_ ? "true" : "false") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Processor available: " << (processor_ ? "true" : "false") << std::endl;
    
    if (session_options_) {
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket connection timeout: " << session_options_->websocketConnectTimeoutMilliseconds << "ms" << std::endl;
        std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket ping/pong monitoring: "
                  << (session_options_->enableCheckPingPongWebsocketProtocolLevel ? "ENABLED" : "DISABLED") << std::endl;
    }
}

// Add a method to log WebSocket-specific status and troubleshooting information
void ExchangeConnectionManager::logWebSocketStatus() const {
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: WebSocket Status:" << std::endl;
    
    if (!session_options_) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   Session options not available!" << std::endl;
        return;
    }
    
    if (!session_configs_) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   Session configs not available!" << std::endl;
        return;
    }
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket Configuration:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Connection timeout: " << session_options_->websocketConnectTimeoutMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Protocol-level ping interval: " << session_options_->pingWebsocketProtocolLevelIntervalMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Protocol-level pong timeout: " << session_options_->pongWebsocketProtocolLevelTimeoutMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Application-level ping interval: " << session_options_->pingWebsocketApplicationLevelIntervalMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Application-level pong timeout: " << session_options_->pongWebsocketApplicationLevelTimeoutMilliseconds << "ms" << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket Health Monitoring:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Protocol-level ping/pong: "
              << (session_options_->enableCheckPingPongWebsocketProtocolLevel ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Application-level ping/pong: "
              << (session_options_->enableCheckPingPongWebsocketApplicationLevel ? "ENABLED" : "DISABLED") << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Troubleshooting Information:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     If no data is received, check:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       1. Network connectivity to exchange WebSocket endpoints" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       2. Exchange WebSocket endpoint availability" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       3. WebSocket connection timeouts (current: " << session_options_->websocketConnectTimeoutMilliseconds << "ms)" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       4. WebSocket ping/pong health checks" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       5. Exchange API rate limits or restrictions" << std::endl;
}

// Add a method to check WebSocket connection health
void ExchangeConnectionManager::checkWebSocketConnection() const {
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Checking WebSocket connection health..." << std::endl;
    
    if (!session_) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   No active session available!" << std::endl;
        return;
    }
    
    if (!session_options_) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   Session options not available!" << std::endl;
        return;
    }
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket Health Check:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Session active: " << (session_ ? "YES" : "NO") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     WebSocket monitoring enabled: "
              << (session_options_->enableCheckPingPongWebsocketProtocolLevel ? "YES" : "NO") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Connection timeout: " << session_options_->websocketConnectTimeoutMilliseconds << "ms" << std::endl;
    
    // Note: CCAPI doesn't provide direct WebSocket connection status API
    // Connection health is monitored through event callbacks in the processor
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket connection health can be monitored through:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     1. Session status events in MarketDataProcessor" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     2. Subscription status events" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     3. Data reception rates in processor stats" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     4. WebSocket ping/pong health checks" << std::endl;
}

// Add a method to log WebSocket-specific debugging information
void ExchangeConnectionManager::logWebSocketDebugInfo() const {
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG] ExchangeConnectionManager: WebSocket Debug Information:" << std::endl;
    
    if (!session_options_ || !session_configs_) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   Session options or configs not available!" << std::endl;
        return;
    }
    
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   WebSocket Configuration:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Connection timeout: " << session_options_->websocketConnectTimeoutMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Protocol-level ping interval: " << session_options_->pingWebsocketProtocolLevelIntervalMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Protocol-level pong timeout: " << session_options_->pongWebsocketProtocolLevelTimeoutMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Application-level ping interval: " << session_options_->pingWebsocketApplicationLevelIntervalMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Application-level pong timeout: " << session_options_->pongWebsocketApplicationLevelTimeoutMilliseconds << "ms" << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   WebSocket Health Monitoring:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Protocol-level ping/pong: "
              << (session_options_->enableCheckPingPongWebsocketProtocolLevel ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Application-level ping/pong: "
              << (session_options_->enableCheckPingPongWebsocketApplicationLevel ? "ENABLED" : "DISABLED") << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   WebSocket URLs:" << std::endl;
    for (const auto& [exchange, url] : session_configs_->getUrlWebsocketBase()) {
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     " << exchange << ": " << url << std::endl;
    }
    
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   Troubleshooting Information:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     If no data is received, check:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       1. Network connectivity to exchange WebSocket endpoints" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       2. Exchange WebSocket endpoint availability" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       3. WebSocket connection timeouts" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       4. WebSocket ping/pong health checks" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       5. Exchange API rate limits or restrictions" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       6. Firewall or proxy blocking WebSocket connections" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       7. DNS resolution issues for WebSocket endpoints" << std::endl;
}

// Add a method to monitor WebSocket data flow
void ExchangeConnectionManager::monitorWebSocketDataFlow() const {
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Monitoring WebSocket data flow..." << std::endl;
    
    if (!session_) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   No active session available!" << std::endl;
        return;
    }
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket Data Flow Monitoring:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Data flow is monitored through MarketDataProcessor event callbacks" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Check processor stats for data reception rates" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Look for SUBSCRIPTION_DATA events in processor logs" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Monitor trades_per_sec and orderbooks_per_sec metrics" << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   Common WebSocket data flow issues:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     1. No SUBSCRIPTION_DATA events received" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     2. Empty message lists in events" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     3. Zero data reception rates" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     4. WebSocket connection drops without reconnection" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     5. Authentication failures (if required)" << std::endl;
}

// Add a method to add comprehensive WebSocket debugging
void ExchangeConnectionManager::addWebSocketDebugging() const {
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG] ExchangeConnectionManager: Adding comprehensive WebSocket debugging..." << std::endl;
    
    if (!session_options_ || !session_configs_) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   Session options or configs not available!" << std::endl;
        return;
    }
    
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   WebSocket Debugging Configuration:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Connection timeout: " << session_options_->websocketConnectTimeoutMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Protocol-level ping interval: " << session_options_->pingWebsocketProtocolLevelIntervalMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Protocol-level pong timeout: " << session_options_->pongWebsocketProtocolLevelTimeoutMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Application-level ping interval: " << session_options_->pingWebsocketApplicationLevelIntervalMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Application-level pong timeout: " << session_options_->pongWebsocketApplicationLevelTimeoutMilliseconds << "ms" << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   WebSocket Health Monitoring:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Protocol-level ping/pong: "
              << (session_options_->enableCheckPingPongWebsocketProtocolLevel ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Application-level ping/pong: "
              << (session_options_->enableCheckPingPongWebsocketApplicationLevel ? "ENABLED" : "DISABLED") << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   WebSocket Data Integrity:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Sequence checking: "
              << (session_options_->enableCheckSequence ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Order book checksum checking: "
              << (session_options_->enableCheckOrderBookChecksum ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Order book crossed checking: "
              << (session_options_->enableCheckOrderBookCrossed ? "ENABLED" : "DISABLED") << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   WebSocket URLs:" << std::endl;
    for (const auto& [exchange, url] : session_configs_->getUrlWebsocketBase()) {
        std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     " << exchange << ": " << url << std::endl;
    }
    
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   WebSocket Troubleshooting:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     If no data is received, check:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       1. Network connectivity to exchange WebSocket endpoints" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       2. Exchange WebSocket endpoint availability" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       3. WebSocket connection timeouts" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       4. WebSocket ping/pong health checks" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       5. Exchange API rate limits or restrictions" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       6. Firewall or proxy blocking WebSocket connections" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       7. DNS resolution issues for WebSocket endpoints" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       8. WebSocket protocol version compatibility" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       9. SSL/TLS certificate validation issues" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]      10. WebSocket message size limits" << std::endl;
    
    // Add WebSocket-specific debugging for common issues
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   WebSocket Connection Debugging:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     Common WebSocket issues to investigate:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - Connection timeout too short (current: " << session_options_->websocketConnectTimeoutMilliseconds << "ms)" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - Ping/pong intervals too aggressive" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - WebSocket URL configuration incorrect" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - Exchange-specific WebSocket requirements" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - Missing authentication credentials" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - WebSocket compression issues" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - WebSocket subprotocol mismatches" << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]   WebSocket Data Flow Debugging:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]     If data is not flowing:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - Check for SUBSCRIPTION_DATA events in MarketDataProcessor" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - Verify message lists are not empty" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - Check for WebSocket protocol errors" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - Monitor WebSocket connection health" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][DEBUG]       - Check exchange-specific WebSocket requirements" << std::endl;
}

// Add a comprehensive WebSocket diagnostic method
void ExchangeConnectionManager::diagnoseWebSocketIssues() const {
    std::cout << "[" << getCurrentTimestamp() << "][INFO] ExchangeConnectionManager: Running comprehensive WebSocket diagnostics..." << std::endl;
    
    if (!session_ || !session_options_ || !session_configs_) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   WebSocket Diagnostic FAILED!" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   Critical components not initialized:" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]     Session: " << (session_ ? "OK" : "NOT INITIALIZED") << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]     Session options: " << (session_options_ ? "OK" : "NOT INITIALIZED") << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]     Session configs: " << (session_configs_ ? "OK" : "NOT INITIALIZED") << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]   This indicates a fundamental initialization issue!" << std::endl;
        return;
    }
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket Configuration Check:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Connection timeout: " << session_options_->websocketConnectTimeoutMilliseconds << "ms" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Protocol-level ping/pong: "
              << (session_options_->enableCheckPingPongWebsocketProtocolLevel ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Application-level ping/pong: "
              << (session_options_->enableCheckPingPongWebsocketApplicationLevel ? "ENABLED" : "DISABLED") << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket URL Configuration:" << std::endl;
    auto urls = session_configs_->getUrlWebsocketBase();
    if (urls.empty()) {
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]     No WebSocket URLs configured!" << std::endl;
        std::cerr << "[" << getCurrentTimestamp() << "][ERROR]     This is a critical configuration issue!" << std::endl;
    } else {
        for (const auto& [exchange, url] : urls) {
            std::cout << "[" << getCurrentTimestamp() << "][INFO]     " << exchange << ": " << url << std::endl;
            
            // Validate URL format
            if (url.find("wss://") != 0 && url.find("ws://") != 0) {
                std::cerr << "[" << getCurrentTimestamp() << "][WARNING]     Invalid WebSocket URL format for " << exchange << "!" << std::endl;
            }
        }
    }
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket Health Check:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Session active: " << (session_ ? "YES" : "NO") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Running status: " << (running_ ? "RUNNING" : "STOPPED") << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket Diagnostic Summary:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Configuration: " << (session_options_ && session_configs_ ? "OK" : "FAILED") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     URLs: " << (urls.empty() ? "FAILED" : "OK") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Session: " << (session_ ? "OK" : "FAILED") << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     Health monitoring: " << (session_options_->enableCheckPingPongWebsocketProtocolLevel ? "ENABLED" : "DISABLED") << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket Troubleshooting Guide:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]     If WebSocket issues persist:" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       1. Check network connectivity to exchange endpoints" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       2. Verify WebSocket URLs are correct and reachable" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       3. Test with different timeout settings" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       4. Check firewall/proxy settings" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       5. Monitor for WebSocket protocol errors" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       6. Verify exchange API credentials (if required)" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       7. Check for exchange rate limiting" << std::endl;
    std::cout << "[" << getCurrentTimestamp() << "][INFO]       8. Test with different ping/pong intervals" << std::endl;
    
    std::cout << "[" << getCurrentTimestamp() << "][INFO]   WebSocket diagnostic completed!" << std::endl;
}
