#!/usr/bin/env bash
# Firewall UFW de srv-domotica: denegar entrante, permitir saliente
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow OpenSSH
sudo ufw allow from 192.168.1.0/24 to any port 53
for p in 5432 27017 1883 1880 3000; do
  sudo ufw allow from 192.168.1.0/24 to any port "$p" proto tcp
done
sudo ufw allow 'Nginx HTTP'
sudo ufw enable
sudo ufw status numbered
