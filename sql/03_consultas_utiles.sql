-- Últimas lecturas recibidas del ESP32
SELECT sensor, variable, valor, unidad, fecha
FROM lecturas ORDER BY fecha DESC LIMIT 10;

-- Lecturas por variable en los últimos 5 minutos (cada 5 s → ~60 por variable)
SELECT variable, count(*) FROM lecturas
WHERE fecha > now() - interval '5 minutes' GROUP BY variable;

-- Promedio por hora de la temperatura
SELECT date_trunc('hour', fecha) AS hora, round(avg(valor), 1) AS promedio
FROM lecturas WHERE variable = 'temperatura'
GROUP BY 1 ORDER BY 1 DESC LIMIT 24;
