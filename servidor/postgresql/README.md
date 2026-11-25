# PostgreSQL 16

Clúster recreado en el disco de datos:

```bash
sudo pg_dropcluster 16 main --stop
sudo mkdir -p /datos/postgresql && sudo chown postgres:postgres /datos/postgresql
sudo pg_createcluster 16 main -d /datos/postgresql/16/main --start
```

- `postgresql.conf`: `listen_addresses = '*'`
- `pg_hba.conf` (línea añadida): ver [`pg_hba.conf.linea`](pg_hba.conf.linea)
- Esquema y usuario de Grafana: [`../../sql`](../../sql)
