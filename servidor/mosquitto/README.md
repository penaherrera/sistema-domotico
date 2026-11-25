# Mosquitto

El archivo `passwd` **no está en el repositorio**. Se crea en el servidor con:

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd esp32     # pide la clave
sudo mosquitto_passwd    /etc/mosquitto/passwd nodered
sudo chown root:mosquitto /etc/mosquitto/passwd /etc/mosquitto/acl
sudo chmod 640 /etc/mosquitto/passwd /etc/mosquitto/acl
sudo systemctl restart mosquitto
```
