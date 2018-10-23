// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018, The Karai Developers
//
// Please see the included LICENSE file for more information.

#include <cxxopts.hpp>
#include <config/CliHeader.h>

#include "DaemonConfiguration.h"
#include "DaemonCommandsHandler.h"
#include "Common/ScopeExit.h"
#include "Common/SignalHandler.h"
#include "Common/StdOutputStream.h"
#include "Common/StdInputStream.h"
#include "Common/PathTools.h"
#include "Common/Util.h"
#include "crypto/hash.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/DatabaseBlockchainCache.h"
#include "CryptoNoteCore/DatabaseBlockchainCacheFactory.h"
#include "CryptoNoteCore/MainChainStorage.h"
#include "CryptoNoteCore/RocksDBWrapper.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "P2p/NetNode.h"
#include "P2p/NetNodeConfig.h"
#include "Rpc/RpcServer.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/BinaryOutputStreamSerializer.h"

#include <config/CryptoNoteCheckpoints.h>
#include <Logging/LoggerManager.h>

#if defined(WIN32)
#include <crtdbg.h>
#include <io.h>
#else
#include <unistd.h>
#endif

using Common::JsonValue;
using namespace CryptoNote;
using namespace Logging;

void print_genesis_tx_hex(const std::vector<std::string> rewardAddresses, const bool blockExplorerMode, LoggerManager& logManager)
{
  std::vector<CryptoNote::AccountPublicAddress> rewardTargets;

  CryptoNote::CurrencyBuilder currencyBuilder(logManager);
  currencyBuilder.isBlockexplorer(blockExplorerMode);

  CryptoNote::Currency currency = currencyBuilder.currency();

  for (const auto& rewardAddress : rewardAddresses)
  {
    CryptoNote::AccountPublicAddress address;
    if (!currency.parseAccountAddressString(rewardAddress, address))
    {
      std::cout << "Failed to parse genesis reward address: " << rewardAddress << std::endl;
      return;
    }
    rewardTargets.emplace_back(std::move(address));
  }

  CryptoNote::Transaction transaction;

  if (rewardTargets.empty())
  {
    if (CryptoNote::parameters::GENESIS_BLOCK_REWARD > 0)
    {
      std::cout << "Error: Genesis Block Reward Addresses are not defined" << std::endl;
      return;
    }
    transaction = CryptoNote::CurrencyBuilder(logManager).generateGenesisTransaction();
  }
  else
  {
    transaction = CryptoNote::CurrencyBuilder(logManager).generateGenesisTransaction();
  }

  std::string transactionHex = Common::toHex(CryptoNote::toBinaryArray(transaction));
  std::cout << getProjectCLIHeader() << std::endl << std::endl
    << "Replace the current GENESIS_COINBASE_TX_HEX line in src/config/CryptoNoteConfig.h with this one:" << std::endl
    << "const char GENESIS_COINBASE_TX_HEX[] = \"" << transactionHex << "\";" << std::endl;

  return;
}

JsonValue buildLoggerConfiguration(Level level, const std::string& logfile) {
  JsonValue loggerConfiguration(JsonValue::OBJECT);
  loggerConfiguration.insert("globalLevel", static_cast<int64_t>(level));

  JsonValue& cfgLoggers = loggerConfiguration.insert("loggers", JsonValue::ARRAY);

  JsonValue& fileLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
  fileLogger.insert("type", "file");
  fileLogger.insert("filename", logfile);
  fileLogger.insert("level", static_cast<int64_t>(TRACE));

  JsonValue& consoleLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
  consoleLogger.insert("type", "console");
  consoleLogger.insert("level", static_cast<int64_t>(TRACE));
  consoleLogger.insert("pattern", "%D %T %L ");

  return loggerConfiguration;
}

/* Wait for input so users can read errors before the window closes if they
   launch from a GUI rather than a terminal */
void pause_for_input(int argc) {
  /* if they passed arguments they're probably in a terminal so the errors will
     stay visible */
  if (argc == 1) {
    #if defined(WIN32)
    if (_isatty(_fileno(stdout)) && _isatty(_fileno(stdin))) {
    #else
    if(isatty(fileno(stdout)) && isatty(fileno(stdin))) {
    #endif
      std::cout << "Press any key to close the program: ";
      getchar();
    }
  }
}

int main(int argc, char* argv[])
{
  std::string configFile, outputFile;
  std::vector<std::string> genesisAwardAddresses;
  bool help, version, osVersion, printGenesisTx, dumpConfig;

  DaemonConfiguration config = initConfiguration(argv[0]);

#ifdef WIN32
  _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

  LoggerManager logManager;
  LoggerRef logger(logManager, "daemon");

  cxxopts::Options options(argv[0], getProjectCLIHeader());

  options.add_options("Core")
    ("help", "Display this help message", cxxopts::value<bool>(help)->implicit_value("true"))
    ("os-version", "Output Operating System version information", cxxopts::value<bool>(osVersion)->default_value("false")->implicit_value("true"))
    ("version","Output daemon version information",cxxopts::value<bool>(version)->default_value("false")->implicit_value("true"));

  options.add_options("Genesis Block")
    ("genesis-block-reward-address", "Specify the address for any premine genesis block rewards", cxxopts::value<std::vector<std::string>>(genesisAwardAddresses), "<address>")
    ("print-genesis-tx", "Print the genesis block transaction hex and exits", cxxopts::value<bool>(printGenesisTx)->default_value("false")->implicit_value("true"));

  options.add_options("Daemon")
    ("c,config-file", "Specify the <path> to a configuration file", cxxopts::value<std::string>(configFile), "<path>")
    ("data-dir", "Specify the <path> to the Blockchain data directory", cxxopts::value<std::string>()->default_value(config.dataDirectory), "<path>")
    ("dump-config", "Prints the current configuration to the screen", cxxopts::value<bool>(dumpConfig)->default_value("false")->implicit_value("true"))
    ("load-checkpoints", "Specify a file <path> containing a CSV of Blockchain checkpoints for faster sync. A value of 'default' uses the built-in checkpoints.",
      cxxopts::value<std::string>()->default_value(config.checkPoints)->implicit_value("default"), "<path>")
    ("log-file", "Specify the <path> to the log file", cxxopts::value<std::string>()->default_value(config.logFile), "<path>")
    ("log-level", "Specify log level", cxxopts::value<int>()->default_value(std::to_string(config.logLevel)), "#")
    ("no-console", "Disable daemon console commands", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
    ("save-config", "Save the configuration to the specified <file>", cxxopts::value<std::string>(outputFile), "<file>");

  options.add_options("RPC")
    ("enable-blockexplorer", "Enable the Blockchain Explorer RPC", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
    ("enable-cors", "Adds header 'Access-Control-Allow-Origin' to the RPC responses using the <domain>. Uses the value specified as the domain. Use * for all.",
      cxxopts::value<std::vector<std::string>>()->implicit_value("*"), "<domain>")
    ("fee-address", "Sets the convenience charge <address> for light wallets that use the daemon", cxxopts::value<std::string>(), "<address>")
    ("fee-amount", "Sets the convenience charge amount for light wallets that use the daemon", cxxopts::value<int>()->default_value("0"), "#");

  options.add_options("Network")
    ("allow-local-ip", "Allow the local IP to be added to the peer list", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
    ("hide-my-port", "Do not announce yourself as a peerlist candidate", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
    ("p2p-bind-ip", "Interface IP address for the P2P service", cxxopts::value<std::string>()->default_value(config.p2pInterface), "<ip>")
    ("p2p-bind-port", "TCP port for the P2P service", cxxopts::value<int>()->default_value(std::to_string(config.p2pPort)), "#")
    ("p2p-external-port", "External TCP port for the P2P service (NAT port forward)", cxxopts::value<int>()->default_value("0"), "#")
    ("rpc-bind-ip", "Interface IP address for the RPC service", cxxopts::value<std::string>()->default_value(config.rpcInterface), "<ip>")
    ("rpc-bind-port", "TCP port for the RPC service", cxxopts::value<int>()->default_value(std::to_string(config.rpcPort)), "#");

  options.add_options("Peer")
    ("add-exclusive-node", "Manually add a peer to the local peer list ONLY attempt connections to it. [ip:port]", cxxopts::value<std::vector<std::string>>(), "<ip:port>")
    ("add-peer", "Manually add a peer to the local peer list", cxxopts::value<std::vector<std::string>>(), "<ip:port>")
    ("add-priority-node", "Manually add a peer to the local peer list and attempt to maintain a connection to it [ip:port]", cxxopts::value<std::vector<std::string>>(), "<ip:port>")
    ("seed-node", "Connect to a node to retrieve the peer list and then disconnect", cxxopts::value<std::vector<std::string>>(), "<ip:port>");

  options.add_options("Database")
    ("db-max-open-files", "Number of files that can be used by the database at one time", cxxopts::value<int>()->default_value(std::to_string(config.dbMaxOpenFiles)), "#")
    ("db-read-buffer-size", "Size of the database read cache in megabytes (MB)", cxxopts::value<int>()->default_value(std::to_string(config.dbReadCacheSize)), "#")
    ("db-threads", "Number of background threads used for compaction and flush operations", cxxopts::value<int>()->default_value(std::to_string(config.dbThreads)), "#")
    ("db-write-buffer-size", "Size of the database write buffer in megabytes (MB)", cxxopts::value<int>()->default_value(std::to_string(config.dbWriteBufferSize)), "#");

  auto &result = [&]() -> auto {
    try
    {
      return options.parse(argc, argv);
    }
    catch (const cxxopts::OptionException& e)
    {
      std::cout << "Error: Unable to parse command line argument options: " << e.what() << std::endl << std::endl
        << options.help({}) << std::endl;
      exit(1);
    }
  }();

  if (help) // Do we want to display the help message?
  {
    std::cout << options.help({}) << std::endl;
    exit(0);
  }
  else if (version) // Do we want to display the software version?
  {
    std::cout << getProjectCLIHeader() << std::endl;
    exit(0);
  }
  else if (osVersion) // Do we want to display the OS version information?
  {
    std::cout << getProjectCLIHeader() << "OS: " << Tools::get_os_version_string() << std::endl;
    exit(0);
  }
  else if (printGenesisTx) // Do we weant to generate the Genesis Tx?
  {
    print_genesis_tx_hex(genesisAwardAddresses, false, logManager);
    exit(0);
  }

  // If the user passed in the --config-file option, we need to handle that first
  if (!configFile.empty())
  {
    try
    {
      handleSettings(configFile, config);
    }
    catch (std::exception& e)
    {
      std::cout << std::endl << "There was an error parsing the specified configuration file. Please check the file and try again"
        << std::endl << e.what() << std::endl;
      exit(1);
    }
  }

  // Load in the CLI specified parameters
  handleSettings(result, config);

  if (dumpConfig)
  {
    std::cout << getProjectCLIHeader() << asString(config) << std::endl;
    exit(0);
  }
  else if (!outputFile.empty())
  {
    try {
      asFile(config, outputFile);
      std::cout << getProjectCLIHeader() << "Configuration saved to: " << outputFile << std::endl;
      exit(0);
    }
    catch (std::exception& e)
    {
      std::cout << getProjectCLIHeader() << "Could not save configuration to: " << outputFile
        << std::endl << e.what() << std::endl;
      exit(1);
    }
  }

  try
  {
    auto modulePath = Common::NativePathToGeneric(argv[0]);
    auto cfgLogFile = Common::NativePathToGeneric(config.logFile);

    if (cfgLogFile.empty()) {
      cfgLogFile = Common::ReplaceExtenstion(modulePath, ".log");
    } else {
      if (!Common::HasParentPath(cfgLogFile)) {
  cfgLogFile = Common::CombinePath(Common::GetPathDirectory(modulePath), cfgLogFile);
      }
    }

    Level cfgLogLevel = static_cast<Level>(static_cast<int>(Logging::ERROR) + config.logLevel);

    // configure logging
    logManager.configure(buildLoggerConfiguration(cfgLogLevel, cfgLogFile));

    logger(INFO, BRIGHT_GREEN) << getProjectCLIHeader() << std::endl;

    logger(INFO) << "Program Working Directory: " << argv[0];

    //create objects and link them
    CryptoNote::CurrencyBuilder currencyBuilder(logManager);
    currencyBuilder.isBlockexplorer(config.enableBlockExplorer);

    try {
      currencyBuilder.currency();
    } catch (std::exception&) {
      std::cout << "GENESIS_COINBASE_TX_HEX constant has an incorrect value. Please launch: " << CryptoNote::CRYPTONOTE_NAME << "d --print-genesis-tx" << std::endl;
      return 1;
    }
    CryptoNote::Currency currency = currencyBuilder.currency();

    bool use_checkpoints = !config.checkPoints.empty();
    CryptoNote::Checkpoints checkpoints(logManager);

    if (use_checkpoints) {
      logger(INFO) << "Loading Checkpoints for faster initial sync...";
      if (config.checkPoints == "default")
      {
        for (const auto& cp : CryptoNote::CHECKPOINTS)
        {
          checkpoints.addCheckpoint(cp.index, cp.blockId);
        }
          logger(INFO) << "Loaded " << CryptoNote::CHECKPOINTS.size() << " default checkpoints";
      }
      else
      {
        bool results = checkpoints.loadCheckpointsFromFile(config.checkPoints);
        if (!results) {
          throw std::runtime_error("Failed to load checkpoints");
        }
      }
    }

    NetNodeConfig netNodeConfig;
    netNodeConfig.init(config.p2pInterface, config.p2pPort, config.p2pExternalPort, config.localIp,
      config.hideMyPort, config.dataDirectory, config.peers,
      config.exclusiveNodes, config.priorityNodes,
      config.seedNodes);

    DataBaseConfig dbConfig;
    dbConfig.init(config.dataDirectory, config.dbThreads, config.dbMaxOpenFiles, config.dbWriteBufferSize, config.dbReadCacheSize);

    if (dbConfig.isConfigFolderDefaulted())
    {
      if (!Tools::create_directories_if_necessary(dbConfig.getDataDir()))
      {
        throw std::runtime_error("Can't create directory: " + dbConfig.getDataDir());
      }
    }
    else
    {
      if (!Tools::directoryExists(dbConfig.getDataDir()))
      {
        throw std::runtime_error("Directory does not exist: " + dbConfig.getDataDir());
      }
    }

    RocksDBWrapper database(logManager);
    database.init(dbConfig);
    Tools::ScopeExit dbShutdownOnExit([&database] () { database.shutdown(); });

    if (!DatabaseBlockchainCache::checkDBSchemeVersion(database, logManager))
    {
      dbShutdownOnExit.cancel();
      database.shutdown();

      database.destroy(dbConfig);

      database.init(dbConfig);
      dbShutdownOnExit.resume();
    }

    System::Dispatcher dispatcher;
    logger(INFO) << "Initializing core...";
    CryptoNote::Core ccore(
      currency,
      logManager,
      std::move(checkpoints),
      dispatcher,
      std::unique_ptr<IBlockchainCacheFactory>(new DatabaseBlockchainCacheFactory(database, logger.getLogger())),
      createSwappedMainChainStorage(config.dataDirectory, currency));

    ccore.load();
    logger(INFO) << "Core initialized OK";

    CryptoNote::CryptoNoteProtocolHandler cprotocol(currency, dispatcher, ccore, nullptr, logManager);
    CryptoNote::NodeServer p2psrv(dispatcher, cprotocol, logManager);
    CryptoNote::RpcServer rpcServer(dispatcher, logManager, ccore, p2psrv, cprotocol);

    cprotocol.set_p2p_endpoint(&p2psrv);
    DaemonCommandsHandler dch(ccore, p2psrv, logManager, &rpcServer);
    logger(INFO) << "Initializing p2p server...";
    if (!p2psrv.init(netNodeConfig))
    {
      logger(ERROR, BRIGHT_RED) << "Failed to initialize p2p server.";
      return 1;
    }

    logger(INFO) << "P2p server initialized OK";

    if (!config.noConsole)
    {
      dch.start_handling();
    }

    // Fire up the RPC Server
    logger(INFO) << "Starting core rpc server on address " << config.rpcInterface << ":" << config.rpcPort;
    rpcServer.start(config.rpcInterface, config.rpcPort);
    rpcServer.setFeeAddress(config.feeAddress);
    rpcServer.setFeeAmount(config.feeAmount);
    rpcServer.enableCors(config.enableCors);
    logger(INFO) << "Core rpc server started ok";

    Tools::SignalHandler::install([&dch, &p2psrv] {
      dch.stop_handling();
      p2psrv.sendStopSignal();
    });

    logger(INFO) << "Starting p2p net loop...";
    p2psrv.run();
    logger(INFO) << "p2p net loop stopped";

    dch.stop_handling();

    //stop components
    logger(INFO) << "Stopping core rpc server...";
    rpcServer.stop();

    //deinitialize components
    logger(INFO) << "Deinitializing p2p...";
    p2psrv.deinit();

    cprotocol.set_p2p_endpoint(nullptr);
    ccore.save();

  }
  catch (const std::exception& e)
  {
    logger(ERROR, BRIGHT_RED) << "Exception: " << e.what();
    return 1;
  }

  logger(INFO) << "Node stopped.";
  return 0;
}
