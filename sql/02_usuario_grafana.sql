-- Usuario de solo lectura para Grafana.
-- La contraseña se define al ejecutar (no se guarda en el repositorio):
--   sudo -u postgres psql -d domotica_db -v clave="'TU_CLAVE'" -f 02_usuario_grafana.sql

CREATE ROLE grafana_lector LOGIN PASSWORD :clave;
GRANT CONNECT ON DATABASE domotica_db TO grafana_lector;
GRANT USAGE ON SCHEMA public TO grafana_lector;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO grafana_lector;
ALTER DEFAULT PRIVILEGES FOR ROLE domotica IN SCHEMA public GRANT SELECT ON TABLES TO grafana_lector;
