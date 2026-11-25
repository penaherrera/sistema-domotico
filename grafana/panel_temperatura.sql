-- Grafana 13 · data source PostgreSQL (usuario grafana_lector, solo lectura)
-- Dashboard "Sistema domótico — Monitoreo" · panel "Temperatura DHT11 (°C)"
-- Time series, umbral rojo en 30 °C
SELECT fecha AS "time", valor AS "Temperatura"
FROM lecturas
WHERE sensor = 'DHT11' AND variable = 'temperatura' AND $__timeFilter(fecha)
ORDER BY fecha;
