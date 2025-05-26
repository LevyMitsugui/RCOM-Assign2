#!/bin/bash

# Mensagem inicial
echo "Iniciando configuração de rede..."

# Restart networking service
echo "Reiniciando o serviço de rede..."
systemctl restart networking

# Configure eth1 interface
echo "Configurando a interface eth1 com IP 172.16.51.1..."
ifconfig eth1 up
ifconfig eth1 172.16.51.1/24

# Add routes
echo "Adicionando rota para a rede 172.16.50.1/24 via gateway 172.16.51.253 (TUX4 eth1)..."
route add -net 172.16.50.0/24 gw 172.16.51.253 # TUX4 eth1

echo "Adicionando rota para a rede 172.16.1.0/24 via gateway 172.16.51.254 (router)..."
route add -net 172.16.1.0/24 gw 172.16.51.254 # router
route -n
# Mensagem final
echo "Configuração de rede concluída!"
