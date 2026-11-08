#pragma once
#include <string>
// Inicia el servicio en el propio dispositivo, escuchando solo en loopback.
std::string xuper_start_local_gateway(const std::string& config,const std::string& key,int port);
