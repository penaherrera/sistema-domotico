// MongoDB 8.0 · base domotica_db
// Ejecutar con: mongosh "mongodb://domotica@localhost:27017/domotica_db" 01_colecciones.js
// (mongosh pide la contraseña; no se guarda aquí)

// eventos: alertas del ESP32, errores, órdenes del dashboard y cambios online/offline
db.createCollection("eventos");
db.eventos.createIndex({ fecha: -1 });
db.eventos.createIndex({ tipo: 1, fecha: -1 });

// estado_actuadores: un documento por actuador, Node-RED hace upsert al cambiar
db.createCollection("estado_actuadores");
["rele", "buzzer", "ventilador"].forEach(id =>
  db.estado_actuadores.updateOne({ _id: id },
    { $setOnInsert: { estado: "apagado", modo: "auto" } }, { upsert: true }));
db.estado_actuadores.updateOne({ _id: "servo" },
  { $setOnInsert: { angulo: 0, modo: "auto" } }, { upsert: true });
db.estado_actuadores.updateOne({ _id: "sistema" },
  { $setOnInsert: { modo: "auto", seguridad: false } }, { upsert: true });
