# Configuración del servidor `srv-domotica` (Fase 3)

Ubuntu Server 24.04.5 LTS en VirtualBox (4 vCPU, 8 GB RAM, discos de 500 y 250 GB), IP fija `192.168.1.233`, zona DNS `domotica.lan`.

| Carpeta | Archivo en el servidor |
|---|---|
| `netplan/` | `/etc/netplan/01-ip-estatica.yaml` |
| `bind/` | `/etc/bind/named.conf.options`, `named.conf.local`, zonas `db.*` |
| `ufw/` | Reglas del firewall |
| `postgresql/` | Clúster en `/datos/postgresql`, línea de `pg_hba.conf` |
| `mongodb/` | `/etc/mongod.conf` |
| `mosquitto/` | `/etc/mosquitto/conf.d/domotica.conf` y `/etc/mosquitto/acl` |
| `nodered/` | Override de systemd (`userDir /srv/nodered`) |
| `nginx/` | `/etc/nginx/sites-available/domotica` (proxy inverso) |

Ningún archivo de esta carpeta contiene contraseñas. Los archivos sensibles del servidor (`/etc/mosquitto/passwd`, `/srv/nodered/flows_cred.json`) se excluyen del repositorio.
