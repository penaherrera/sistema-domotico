// Crear el usuario de la aplicación (ejecutar como admin).
// passwordPrompt() pide la clave por teclado: no queda escrita en el archivo.
db.getSiblingDB("domotica_db").createUser({
  user: "domotica",
  pwd: passwordPrompt(),
  roles: [{ role: "readWrite", db: "domotica_db" }]
});
