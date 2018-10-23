// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cxxopts.hpp>
#include <json.hpp>
#include <fstream>
#include <sstream>

#include <config/CryptoNoteConfig.h>
#include <Logging/ILogger.h>
#include "Common/PathTools.h"
#include "Common/Util.h"

using nlohmann::json;

namespace {
  struct DaemonConfiguration
  {
    std::string dataDirectory;
    std::string logFile;
    std::string feeAddress;
    std::string rpcInterface;
    std::string p2pInterface;
    std::string checkPoints;

    std::vector<std::string> peers;
    std::vector<std::string> priorityNodes;
    std::vector<std::string> exclusiveNodes;
    std::vector<std::string> seedNodes;
    std::vector<std::string> enableCors;

    int logLevel;
    int feeAmount;
    int rpcPort;
    int p2pPort;
    int p2pExternalPort;
    int dbThreads;
    int dbMaxOpenFiles;
    int dbWriteBufferSize;
    int dbReadCacheSize;

    bool noConsole;
    bool enableBlockExplorer;
    bool localIp;
    bool hideMyPort;
  };

  DaemonConfiguration initConfiguration()
  {
    DaemonConfiguration config;

    std::stringstream logfile;
      logfile << CryptoNote::CRYPTONOTE_NAME << "d.log";

    config.dataDirectory = Tools::getDefaultDataDirectory();
    config.checkPoints = "default";
    config.logFile = logfile.str();
    config.logLevel = Logging::WARNING;
    config.dbMaxOpenFiles = CryptoNote::DATABASE_DEFAULT_MAX_OPEN_FILES;
    config.dbReadCacheSize = CryptoNote::DATABASE_READ_BUFFER_MB_DEFAULT_SIZE;
    config.dbThreads = CryptoNote::DATABASE_DEFAULT_BACKGROUND_THREADS_COUNT;
    config.dbWriteBufferSize = CryptoNote::DATABASE_WRITE_BUFFER_MB_DEFAULT_SIZE;
    config.p2pInterface = "0.0.0.0";
    config.p2pPort = CryptoNote::P2P_DEFAULT_PORT;
    config.p2pExternalPort = 0;
    config.rpcInterface = "127.0.0.1";
    config.rpcPort = CryptoNote::RPC_DEFAULT_PORT;
    config.noConsole = false;
    config.enableBlockExplorer = false;
    config.localIp = false;
    config.hideMyPort = false;

    return config;
  }

  DaemonConfiguration initConfiguration(const char* path)
  {
    DaemonConfiguration config = initConfiguration();

    config.logFile = Common::ReplaceExtenstion(Common::NativePathToGeneric(path), ".log");

    return config;
  };

  void handleSettings(const cxxopts::ParseResult& cli, DaemonConfiguration& config)
  {
    if (cli.count("data-dir") > 0)
    {
      config.dataDirectory = cli["data-dir"].as<std::string>();
    }

    if (cli.count("load-checkpoints") > 0)
    {
      config.checkPoints = cli["load-checkpoints"].as<std::string>();
    }

    if (cli.count("log-file") > 0)
    {
      config.logFile = cli["log-file"].as<std::string>();
    }

    if (cli.count("log-level") > 0)
    {
      config.logLevel = cli["log-level"].as<int>();
    }

    if (cli.count("no-console") > 0)
    {
      config.noConsole = cli["no-console"].as<bool>();
    }

    if (cli.count("db-max-open-files") > 0)
    {
      config.dbMaxOpenFiles = cli["db-max-open-files"].as<int>();
    }

    if (cli.count("db-read-buffer-size") > 0)
    {
      config.dbReadCacheSize = cli["db-read-buffer-size"].as<int>();
    }

    if (cli.count("db-threads") > 0)
    {
      config.dbThreads = cli["db-threads"].as<int>();
    }

    if (cli.count("db-write-buffer-size") > 0)
    {
      config.dbWriteBufferSize = cli["db-write-buffer-size"].as<int>();
    }

    if (cli.count("local-ip") > 0)
    {
      config.localIp = cli["local-ip"].as<bool>();
    }

    if (cli.count("hide-my-port") > 0)
    {
      config.hideMyPort = cli["hide-my-port"].as<bool>();
    }

    if (cli.count("p2p-bind-ip") > 0)
    {
      config.p2pInterface = cli["p2p-bind-ip"].as<std::string>();
    }

    if (cli.count("p2p-bind-port") > 0)
    {
      config.p2pPort = cli["p2p-bind-port"].as<int>();
    }

    if (cli.count("p2p-external-port") > 0)
    {
      config.p2pExternalPort = cli["p2p-external-port"].as<int>();
    }

    if (cli.count("rpc-bind-ip") > 0)
    {
      config.rpcInterface = cli["rpc-bind-ip"].as<std::string>();
    }

    if (cli.count("rpc-bind-port") > 0)
    {
      config.rpcPort = cli["rpc-bind-port"].as<int>();
    }

    if (cli.count("add-exclusive-node") > 0 )
    {
      config.exclusiveNodes = cli["add-exclusive-node"].as<std::vector<std::string>>();
    }

    if (cli.count("add-peer") > 0)
    {
      config.peers = cli["add-peer"].as<std::vector<std::string>>();
    }

    if (cli.count("add-priority-node") > 0)
    {
      config.priorityNodes = cli["add-priority-node"].as<std::vector<std::string>>();
    }

    if (cli.count("seed-node") > 0)
    {
      config.seedNodes = cli["seed-node"].as<std::vector<std::string>>();
    }

    if (cli.count("enable-blockexplorer") > 0)
    {
      config.enableBlockExplorer = cli["enable-blockexplorer"].as<bool>();
    }

    if (cli.count("enable-cors") > 0)
    {
      config.enableCors = cli["enable-cors"].as<std::vector<std::string>>();
    }

    if (cli.count("fee-address") > 0)
    {
      config.feeAddress = cli["fee-address"].as<std::string>();
    }

    if (cli.count("fee-amount") > 0)
    {
      config.feeAmount = cli["fee-amount"].as<int>();
    }
  };

  void handleSettings(const std::string configFile, DaemonConfiguration& config)
  {
    std::ifstream data(configFile.c_str());

    if (!data.good())
    {
      throw std::runtime_error("The --config-file you specified does not exist, please check the filename and try again.");
    }

    json j;
    data >> j;

    if (j.find("data-dir") != j.end())
    {
      config.dataDirectory = j["data-dir"].get<std::string>();
    }

    if (j.find("load-checkpoints") != j.end())
    {
      config.checkPoints = j["load-checkpoints"].get<std::string>();
    }

    if (j.find("log-file") != j.end())
    {
      config.logFile = j["log-file"].get<std::string>();
    }

    if (j.find("log-level") != j.end())
    {
      config.logLevel = j["log-level"].get<int>();
    }

    if (j.find("no-console") != j.end())
    {
      config.noConsole = j["no-console"].get<bool>();
    }

    if (j.find("db-max-open-files") != j.end())
    {
      config.dbMaxOpenFiles = j["db-max-open-files"].get<int>();
    }

    if (j.find("db-read-buffer-size") != j.end())
    {
      config.dbReadCacheSize = j["db-read-buffer-size"].get<int>();
    }

    if (j.find("db-threads") != j.end())
    {
      config.dbThreads = j["db-threads"].get<int>();
    }

    if (j.find("db-write-buffer-size") != j.end())
    {
      config.dbWriteBufferSize = j["db-write-buffer-size"].get<int>();
    }

    if (j.find("allow-local-ip") != j.end())
    {
      config.localIp = j["allow-local-ip"].get<bool>();
    }

    if (j.find("hide-my-port") != j.end())
    {
      config.hideMyPort = j["hide-my-port"].get<bool>();
    }

    if (j.find("p2p-bind-ip") != j.end())
    {
      config.p2pInterface = j["p2p-bind-ip"].get<std::string>();
    }

    if (j.find("p2p-bind-port") != j.end())
    {
      config.p2pPort = j["p2p-bind-port"].get<int>();
    }

    if (j.find("p2p-external-port") != j.end())
    {
      config.p2pExternalPort = j["p2p-external-port"].get<int>();
    }

    if (j.find("rpc-bind-ip") != j.end())
    {
      config.rpcInterface = j["rpc-bind-ip"].get<std::string>();
    }

    if (j.find("rpc-bind-port") != j.end())
    {
      config.rpcPort = j["rpc-bind-port"].get<int>();
    }

    if (j.find("add-exclusive-node") != j.end())
    {
      config.exclusiveNodes = j["add-exclusive-node"].get<std::vector<std::string>>();
    }

    if (j.find("add-peer") != j.end())
    {
      config.peers = j["add-peer"].get<std::vector<std::string>>();
    }

    if (j.find("add-priority-node") != j.end())
    {
      config.priorityNodes = j["add-priority-node"].get<std::vector<std::string>>();
    }

    if (j.find("seed-node") != j.end())
    {
      config.seedNodes = j["seed-node"].get<std::vector<std::string>>();
    }

    if (j.find("enable-blockexplorer") != j.end())
    {
      config.enableBlockExplorer = j["enable-blockexplorer"].get<bool>();
    }

    if (j.find("enable-cors") != j.end())
    {
      config.enableCors = j["enable-cors"].get<std::vector<std::string>>();
    }

    if (j.find("fee-address") != j.end())
    {
      config.feeAddress = j["fee-address"].get<std::string>();
    }

    if (j.find("fee-amount") != j.end())
    {
      config.feeAmount = j["fee-amount"].get<int>();
    }
  };

  json asJSON(const DaemonConfiguration& config)
  {
    json j = json {
      {"data-dir", config.dataDirectory},
      {"load-checkpoints", config.checkPoints},
      {"log-file", config.logFile},
      {"log-level", config.logLevel},
      {"no-console", config.noConsole},
      {"db-max-open-files", config.dbMaxOpenFiles},
      {"db-read-buffer-size", config.dbReadCacheSize},
      {"db-threads", config.dbThreads},
      {"db-write-buffer-size", config.dbWriteBufferSize},
      {"allow-local-ip", config.localIp},
      {"hide-my-port", config.hideMyPort},
      {"p2p-bind-ip", config.p2pInterface},
      {"p2p-bind-port", config.p2pPort},
      {"p2p-external-port", config.p2pExternalPort},
      {"rpc-bind-ip", config.rpcInterface},
      {"rpc-bind-port", config.rpcPort},
      {"add-exclusive-node", config.exclusiveNodes},
      {"add-peer", config.peers},
      {"add-priority-node", config.priorityNodes},
      {"seed-node", config.seedNodes},
      {"enable-blockexplorer", config.enableBlockExplorer},
      {"enable-cors", config.enableCors},
      {"fee-address", config.feeAddress},
      {"fee-amount", config.feeAmount},
    };

    return j;
  };

  std::string asString(const DaemonConfiguration& config)
  {
    json j = asJSON(config);
    return j.dump(2);
  };

  void asFile(const DaemonConfiguration& config, const std::string& filename)
  {
    json j = asJSON(config);
    std::ofstream data(filename);
    data << std::setw(2) << j << std::endl;
  }
}