// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cxxopts.hpp>
#include <json.hpp>
#include <fstream>
#include <string>

#include <config/CryptoNoteConfig.h>
#include <Logging/ILogger.h>

using nlohmann::json;

namespace PaymentService {
  struct WalletServiceConfiguration
  {
    std::string daemonAddress;
    std::string bindAddress;
    std::string rpcPassword;
    std::string containerFile;
    std::string containerPassword;
    std::string serverRoot;
    std::string corsHeader;
    std::string logFile;

    int daemonPort;
    int bindPort;
    int logLevel;

    bool legacySecurity;
  };

  inline WalletServiceConfiguration initConfiguration()
  {
    WalletServiceConfiguration config;

    config.daemonAddress = "127.0.0.1";
    config.bindAddress = "127.0.0.1";
    config.logFile = "service.log";
    config.daemonPort = CryptoNote::RPC_DEFAULT_PORT;
    config.bindPort = CryptoNote::SERVICE_DEFAULT_PORT;
    config.logLevel = Logging::INFO;
    config.legacySecurity = false;

    return config;
  };

  inline void handleSettings(const cxxopts::ParseResult& cli, WalletServiceConfiguration& config)
  {
    if (cli.count("daemon-address") > 0)
    {
      config.daemonAddress = cli["daemon-address"].as<std::string>();
    }

    if (cli.count("daemon-port") > 0)
    {
      config.daemonPort = cli["daemon-port"].as<int>();
    }

    if (cli.count("log-file") > 0)
    {
      config.logFile = cli["log-file"].as<std::string>();
    }

    if (cli.count("log-level") > 0)
    {
      config.logLevel = cli["log-level"].as<int>();
    }

    if (cli.count("container-file") > 0)
    {
      config.containerFile = cli["container-file"].as<std::string>();
    }

    if (cli.count("container-password") > 0)
    {
      config.containerPassword = cli["container-password"].as<std::string>();
    }

    if (cli.count("bind-address") > 0)
    {
      config.bindAddress = cli["bind-address"].as<std::string>();
    }

    if (cli.count("bind-port") > 0)
    {
      config.bindPort = cli["bind-port"].as<int>();
    }

    if (cli.count("enable-cors") > 0)
    {
      config.corsHeader = cli["enable-cors"].as<std::string>();
    }

    if (cli.count("rpc-legacy-security") > 0)
    {
      config.legacySecurity = cli["rpc-legacy-security"].as<bool>();
    }

    if (cli.count("rpc-password") > 0)
    {
      config.rpcPassword = cli["rpc-password"].as<std::string>();
    }

    if (cli.count("server-root") > 0)
    {
      config.serverRoot = cli["server-root"].as<std::string>();
    }
  };

  inline void handleSettings(const std::string configFile, WalletServiceConfiguration& config)
  {
    std::ifstream data(configFile.c_str());

    if (!data.good())
    {
      throw std::runtime_error("The --config-file you specified does not exist, please check the filename and try again.");
    }

    json j;
    data >> j;

    if (j.find("daemon-address") != j.end())
    {
      config.daemonAddress = j["daemon-address"].get<std::string>();
    }

    if (j.find("daemon-port") != j.end())
    {
      config.daemonPort = j["daemon-port"].get<int>();
    }

    if (j.find("log-file") != j.end())
    {
      config.logFile = j["log-file"].get<std::string>();
    }

    if (j.find("log-level") != j.end())
    {
      config.logLevel = j["log-level"].get<int>();
    }

    if (j.find("container-file") != j.end())
    {
      config.containerFile = j["container-file"].get<std::string>();
    }

    if (j.find("container-password") != j.end())
    {
      config.containerPassword = j["container-password"].get<std::string>();
    }

    if (j.find("bind-address") != j.end())
    {
      config.bindAddress = j["bind-address"].get<std::string>();
    }

    if (j.find("bind-port") != j.end())
    {
      config.bindPort = j["bind-port"].get<int>();
    }

    if (j.find("enable-cors") != j.end())
    {
      config.corsHeader = j["enable-cors"].get<std::string>();
    }

    if (j.find("rpc-legacy-security") != j.end())
    {
      config.legacySecurity = j["rpc-legacy-security"].get<bool>();
    }

    if (j.find("rpc-password") != j.end())
    {
      config.rpcPassword = j["rpc-password"].get<std::string>();
    }

    if (j.find("server-root") != j.end())
    {
      config.serverRoot = j["server-root"].get<std::string>();
    }
  }

  inline json asJSON(const WalletServiceConfiguration& config)
  {
    json j = json {
      {"daemon-address", config.daemonAddress},
      {"daemon-port", config.daemonPort},
      {"log-file", config.logFile},
      {"log-level", config.logLevel},
      {"container-file", config.containerFile},
      {"container-password", config.containerPassword},
      {"bind-address", config.bindAddress},
      {"bind-port", config.bindPort},
      {"enable-cors", config.corsHeader},
      {"rpc-legacy-security", config.legacySecurity},
      {"rpc-password", config.rpcPassword},
      {"server-root", config.serverRoot},
    };

    return j;
  };

  inline std::string asString(const WalletServiceConfiguration& config)
  {
    json j = asJSON(config);
    return j.dump(2);
  };

  inline void asFile(const WalletServiceConfiguration& config, const std::string& filename)
  {
    json j = asJSON(config);
    std::ofstream data(filename);
    data << std::setw(2) << j << std::endl;
  };
}