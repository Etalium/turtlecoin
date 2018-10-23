// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "ConfigurationManager.h"

#include <iostream>
#include <fstream>

#include <CryptoTypes.h>
#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <cxxopts.hpp>

#include "Common/CommandLine.h"
#include "Common/Util.h"
#include "crypto/hash.h"
#include "Logging/ILogger.h"

#include "version.h"

namespace PaymentService {

ConfigurationManager::ConfigurationManager() {
  generateNewContainer = false;
  daemonize = false;
  registerService = false;
  unregisterService = false;
  printAddresses = false;
  secretViewKey = "";
  secretSpendKey = "";
  mnemonicSeed = "";
  rpcSecret = Crypto::Hash();
  scanHeight = 0;
}

bool ConfigurationManager::init(int argc, char** argv)
{
  bool help, version, dumpConfig;
  std::string configFile, outputFile;
  serviceConfig = initConfiguration();

  cxxopts::Options options(argv[0], CryptoNote::getProjectCLIHeader());

  options.add_options("Core")
    ("h,help", "Display this help message", cxxopts::value<bool>(help)->implicit_value("true"))
    ("v,version", "Output software version information", cxxopts::value<bool>(version)->default_value("false")->implicit_value("true"));

  options.add_options("Daemon")
    ("daemon-address", "The daemon host to use for node operations",cxxopts::value<std::string>()->default_value(serviceConfig.daemonAddress), "<ip>")
    ("daemon-port", "The daemon RPC port to use for node operations", cxxopts::value<int>()->default_value(std::to_string(serviceConfig.daemonPort)), "<port>");

  options.add_options("Service")
    ("c,config", "Specify the configuration <file> to use instead of CLI arguments", cxxopts::value<std::string>(configFile), "<file>")
    ("dump-config", "Prints the current configuration to the screen", cxxopts::value<bool>(dumpConfig)->default_value("false")->implicit_value("true"))
    ("log-file", "Specify log <file> location", cxxopts::value<std::string>()->default_value(serviceConfig.logFile), "<file>")
    ("log-level", "Specify log level", cxxopts::value<int>()->default_value(std::to_string(serviceConfig.logLevel)), "#")
#ifdef WIN32
    ("register-service", "Registers this program as a Windows service",cxxopts::value<bool>(registerService)->default_value("false")->implicit_value("true"))
#endif
    ("server-root", "The service will use this <path> as the working directory", cxxopts::value<std::string>(), "<path>")
#ifdef WIN32
    ("unregister-service", "Unregisters this program from being a Windows service", cxxopts::value<bool>(unregisterService)->default_value("false")->implicit_value("true"))
#endif
    ("save-config", "Save the configuration to the specified <file>", cxxopts::value<std::string>(outputFile), "<file>");

  options.add_options("Wallet")
    ("address", "Print the wallet addresses and then exit", cxxopts::value<bool>(printAddresses)->default_value("false")->implicit_value("true"))
    ("w,container-file", "Wallet container <file>", cxxopts::value<std::string>(), "<file>")
    ("p,container-password", "Wallet container <password>", cxxopts::value<std::string>(), "<password>")
    ("g,generate-container", "Generate a new wallet container", cxxopts::value<bool>(generateNewContainer)->default_value("false")->implicit_value("true"))
    ("view-key", "Generate a wallet container with this secret view <key>", cxxopts::value<std::string>(secretViewKey), "<key>")
    ("spend-key", "Generate a wallet container with this secret spend <key>", cxxopts::value<std::string>(secretSpendKey), "<key>")
    ("mnemonic-seed", "Generate a wallet container with this Mnemonic <seed>", cxxopts::value<std::string>(mnemonicSeed), "<seed>")
    ("scan-height", "Start scanning for transactions from this Blockchain height", cxxopts::value<uint64_t>(scanHeight)->default_value("0"), "#")
    ("SYNC_FROM_ZERO", "Force the wallet to sync from 0", cxxopts::value<bool>(syncFromZero)->default_value("false")->implicit_value("true"));

  options.add_options("Network")
    ("bind-address", "Interface IP address for the RPC service", cxxopts::value<std::string>()->default_value(serviceConfig.bindAddress), "<ip>")
    ("bind-port", "TCP port for the RPC service", cxxopts::value<int>()->default_value(std::to_string(serviceConfig.bindPort)), "<port>");

  options.add_options("RPC")
    ("enable-cors", "Adds header 'Access-Control-Allow-Origin' to the RPC responses. Uses the value specified as the domain. Use * for all.",
      cxxopts::value<std::string>(), "<domain>")
    ("rpc-legacy-security", "Enable legacy mode (no password for RPC). WARNING: INSECURE. USE ONLY AS A LAST RESORT.",
      cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
    ("rpc-password", "Specify the <password> to access the RPC server.", cxxopts::value<std::string>(), "<password>");

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
    std::cout << CryptoNote::getProjectCLIHeader() << std::endl;
    exit(0);
  }

  // If the user passed in the --config-file option, we need to handle that first
  if (!configFile.empty())
  {
    try
    {
      handleSettings(configFile, serviceConfig);
    }
    catch (std::exception& e)
    {
      std::cout << std::endl << "There was an error parsing the specified configuration file. Please check the file and try again"
        << std::endl << e.what() << std::endl;
      exit(1);
    }
  }

  // Load in the CLI specified parameters
  handleSettings(result, serviceConfig);

  if (dumpConfig)
  {
    std::cout << CryptoNote::getProjectCLIHeader() << asString(serviceConfig) << std::endl;
    exit(0);
  }
  else if (!outputFile.empty())
  {
    try {
      asFile(serviceConfig, outputFile);
      std::cout << CryptoNote::getProjectCLIHeader() << "Configuration saved to: " << outputFile << std::endl;
      exit(0);
    }
    catch (std::exception& e)
    {
      std::cout << CryptoNote::getProjectCLIHeader() << "Could not save configuration to: " << outputFile
        << std::endl << e.what() << std::endl;
      exit(1);
    }
  }

  if (registerService && unregisterService)
  {
    throw std::runtime_error("It's impossible to use both --register-service and --unregister-service at the same time");
  }

  if (serviceConfig.logLevel > Logging::TRACE)
  {
    throw std::runtime_error("log-level must be between " + std::to_string(Logging::FATAL) +  ".." + std::to_string(Logging::TRACE));
  }

  if (serviceConfig.containerFile.empty())
  {
    throw std::runtime_error("You must specify a wallet file to open!");
  }

  if (!std::ifstream(serviceConfig.containerFile) && !generateNewContainer)
  {
    if (std::ifstream(serviceConfig.containerFile + ".wallet"))
    {
      throw std::runtime_error("The wallet file you specified does not exist. Did you mean: " + serviceConfig.containerFile + ".wallet?");
    }
    else
    {
      throw std::runtime_error("The wallet file you specified does not exist; please check your spelling and try again.");
    }
  }

  if ((!secretViewKey.empty() || !secretSpendKey.empty()) && !generateNewContainer)
  {
    throw std::runtime_error("--generate-container is required");
  }

  if (!mnemonicSeed.empty() && !generateNewContainer)
  {
    throw std::runtime_error("--generate-container is required");
  }

  if (!mnemonicSeed.empty() && (!secretViewKey.empty() || !secretSpendKey.empty()))
  {
    throw std::runtime_error("You cannot specify import from both Mnemonic seed and private keys");
  }

  if ((registerService || unregisterService) && serviceConfig.containerFile.empty())
  {
    throw std::runtime_error("--container-file parameter is required");
  }

  // If we are generating a new container, we can skip additional checks
  if (generateNewContainer)
  {
    return true;
  }

  // Run authentication checks

  if(serviceConfig.rpcPassword.empty() && !serviceConfig.legacySecurity)
  {
    throw std::runtime_error("Please specify either an RPC password or use the --rpc-legacy-security flag");
  }

  if (!serviceConfig.rpcPassword.empty())
  {
    std::vector<uint8_t> rawData(serviceConfig.rpcPassword.begin(), serviceConfig.rpcPassword.end());
    Crypto::cn_slow_hash_v0(rawData.data(), rawData.size(), rpcSecret);
    serviceConfig.rpcPassword = "";
  }

  return true;
}

}