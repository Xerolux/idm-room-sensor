# Service design

Resolve existing register definitions, validate writability, finiteness and
metadata ranges, then perform the requested writes. Register resolution for
temperature and humidity must finish before the first write to prevent a
missing optional humidity register from causing a partial update.
