#!/bin/bash

# Mensagem inicial
echo "Iniciando configuração de rede..."

# Restart networking service
echo "Reiniciando o serviço de rede..."
systemctl restart networking

# Configure eth1 interface
echo "Configurando a interface eth1 e eth2 com IP"
ifconfig eth1 up
ifconfig eth2 up
ifconfig eth1 172.16.50.254/24
ifconfig eth2 172.16.51.253/24

echo "Enable Forward"
sysctl net.ipv4.ip_forward=1

echo "Disable broadcasts"
sysctl net.ipv4.icmp_echo_ignore_broadcasts=0


# Add route for 172.16.1.0/24 via gateway 172.16.51.254
echo "Adicionando rota para a rede 172.16.1.0/24 via gateway 172.16.51.254 (router)..."
route add -net 172.16.1.0/24 gw 172.16.51.254 # router
route -n

# Mensagem final
echo "Configuração de rede concluída!"
