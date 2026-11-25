-- PostgreSQL 16 · base domotica_db (propietario: domotica)
-- Histórico de lecturas de los sensores (lo llena Node-RED, lo lee Grafana)

CREATE TABLE IF NOT EXISTS lecturas (
    id        BIGSERIAL    PRIMARY KEY,
    sensor    VARCHAR(20)  NOT NULL,   -- DHT11, MQ2, KS0048, KS0038
    variable  VARCHAR(20)  NOT NULL,   -- temperatura, humedad, gas, agua, puerta
    valor     NUMERIC(8,2) NOT NULL,
    unidad    VARCHAR(10),             -- °C, %, indice, estado
    fecha     TIMESTAMPTZ  NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_lecturas_fecha  ON lecturas (fecha);
CREATE INDEX IF NOT EXISTS idx_lecturas_sensor ON lecturas (sensor, variable, fecha);
