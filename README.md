# GraphDB

GraphDB is a C++20 graph database project with a parser, a planner pipeline, and a storage layer.

## Components

- `parser` — lexer and parser for query input
- `planner` — logical planner, physical planner, optimizer, and execution runner
- `storage` — graph storage and persistence primitives
- `tests` — GoogleTest-based unit tests

## Build

Requirements:

- CMake 3.25 or newer
- C++20 compiler
- Boost

## Installation:

```bash
git clone https://github.com/PVA325/GraphDB.git
cd GraphDB
cmake -S . -B build -DCMAKE_INSTALL_RPATH='$ORIGIN/../lib' -DCMAKE_BUILD_RPATH='$ORIGIN/../lib' -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=TRUE
cmake --build build -j
sudo cmake --install build
```
You can alternately install into another directory with ```cmake -DCMAKE_INSTALL_PREFIX=<folder> ...```

## Run

```bash
graphdb
```

The runner reads commands terminated by `;` and supports:

- `exit;`
- `flush;`
- `save;`
- `save <path>;`
- `load <path>;`
- `Queries with Cypher syntax`

Examples of queries are at the end

## Authors

- artyyshk — parser + lexer
- PVA325 — logical and physical planner + optimizer + runner
- isachenkostanislav453-ai — storage

## Query Examples

### Create data

```cypher
CREATE
  (alice:Person {
    name: "Alice",
    age: 24,
    city: "Paris",
    score: 120,
    level: 3
  }),
  (bob:Person {
    name: "Bob",
    age: 29,
    city: "London",
    score: 95,
    level: 2
  }),
  (charlie:Person {
    name: "Charlie",
    age: 31,
    city: "Berlin",
    score: 170,
    level: 5
  }),
  (david:Person {
    name: "David",
    age: 27,
    city: "Madrid",
    score: 110,
    level: 2
  }),
  (eve:Person {
    name: "Eve",
    age: 35,
    city: "Paris",
    score: 200,
    level: 6
  }),
  (frank:Person {
    name: "Frank",
    age: 22,
    city: "Rome",
    score: 80,
    level: 1
  }),
  (google:Company {
    name: "Google",
    country: "USA",
    rating: 10
  }),
  (openai:Company {
    name: "OpenAI",
    country: "USA",
    rating: 10
  }),
  (spotify:Company {
    name: "Spotify",
    country: "Sweden",
    rating: 8
  }),
  (paris:City {
    name: "Paris",
    population: 2200000
  }),
  (london:City {
    name: "London",
    population: 8900000
  }),
  (berlin:City {
    name: "Berlin",
    population: 3600000
  }),
  (alice)-[:KNOWS {since: 2019, strength: 9}]->(charlie),
  (alice)-[:KNOWS {since: 2020, strength: 7}]->(bob),
  (bob)-[:KNOWS {since: 2021, strength: 5}]->(david),
  (charlie)-[:KNOWS {since: 2018, strength: 10}]->(eve),
  (eve)-[:KNOWS {since: 2022, strength: 6}]->(frank),
  (frank)-[:KNOWS {since: 2023, strength: 4}]->(alice),
  (alice)-[:WORKS_AT {salary: 5000}]->(google),
  (bob)-[:WORKS_AT {salary: 3500}]->(spotify),
  (charlie)-[:WORKS_AT {salary: 9000}]->(openai),
  (david)-[:WORKS_AT {salary: 4500}]->(google),
  (eve)-[:WORKS_AT {salary: 12000}]->(openai),
  (frank)-[:WORKS_AT {salary: 2000}]->(spotify),
  (alice)-[:LIVES_IN]->(paris),
  (eve)-[:LIVES_IN]->(paris),
  (bob)-[:LIVES_IN]->(london),
  (charlie)-[:LIVES_IN]->(berlin);
```

### Query examples

```cypher
MATCH (a:Person)-[k:KNOWS]->(b:Person)-[w:WORKS_AT]->(c:Company)
RETURN a.name, b.name, c.name, k.strength
ORDER BY k.strength DESC;
```

```cypher
MATCH (a:Person)-[:KNOWS]->(b:Person)-[:KNOWS]->(c:Person)
RETURN a.name, b.name, c.name;
```

```cypher
MATCH
  (p:Person)-[:WORKS_AT]->(c:Company),
  (p)-[:LIVES_IN]->(city:City)
RETURN p.name, c.name, city.name
ORDER BY p.name;
```

```cypher
MATCH
  (p:Person)-[w:WORKS_AT]->(c:Company)
WHERE
  p.score > 100 AND c.country = "USA"
RETURN
  p.name, c.name, w.salary
ORDER BY w.salary DESC;
```

```cypher
MATCH (p:Person)
RETURN p.city, p.name, p.score
ORDER BY p.city ASC, p.score DESC;
```

```cypher
MATCH (a:Person)-[r:KNOWS]->(b:Person)
WHERE r.strength > 6
RETURN a.name, b.name, r.strength
ORDER BY r.strength DESC;
```

```cypher
MATCH
  (a:Person)-[:WORKS_AT]->(c:Company),
  (b:Person)-[:WORKS_AT]->(c:Company)
WHERE a.name < b.name
RETURN a.name, b.name, c.name;
```

```cypher
MATCH
  (a:Person)-[k:KNOWS]->(b:Person),
  (b)-[w:WORKS_AT]->(c:Company),
  (b)-[:LIVES_IN]->(city:City)
WHERE
  k.strength > 5
  AND b.score > 100
  AND c.rating > 8
RETURN
  a.name,
  b.name,
  c.name,
  city.name,
  k.strength,
  w.salary
ORDER BY
  w.salary DESC,
  k.strength DESC
LIMIT 5;
```

### Update & Delete examples

```cypher
MATCH (p:Person)
WHERE p.name = "Frank"
SET p.score = 300;
```

```cypher
MATCH (p:Person)
RETURN p.name, p.score
ORDER BY p.score DESC;
```

```cypher
MATCH (p:Person)
WHERE p.name = "Charlie"
DELETE p;
```

```cypher
MATCH (a:Person)-[:KNOWS]->(b:Person)
RETURN a.name, b.name;
```
