# COMP30023 Project 2 — Battleship (C Implementation)

**Delivery document — task-by-task execution and results**

| Item | Detail |
|------|--------|
| Project | Battleship networked client/server |
| Language | C only (`client_c/`, `server_c/`, `common/`) |
| Location | `~/BattleShip_Proj/project2-main/project2-main/` |
| Test cases | `~/BattleShip_Proj/project2-cases-main/project2-cases-main/` |
| Test date | May 2026 |
| Overall status | **All 20 visible test cases PASS** |

---

## Prerequisites

- Linux with `gcc`, `make`, `ar`
- Prebuilt libraries in `project2-bin/` (`libengine.a`, `librunner.a`)

---

## Build (all tasks)

```bash
cd ~/BattleShip_Proj/project2-main/project2-main
make clean && make -B server.a client.a && make client server
```

**Expected result:** Builds with `-Wall -Wextra -std=c11` and no errors. Produces `./server` and `./client`.

---

## How to run manually (two terminals)

**Terminal 1 — start server (leave running):**
```bash
cd ~/BattleShip_Proj/project2-main/project2-main
fuser -k 30023/tcp 2>/dev/null   # free port if needed
./server 30023
```

**Expected output:**
```text
Battleship server listening on 0.0.0.0:30023 (TCP — use ./client, not a browser)
```

**Terminal 2 — run a test client:**
```bash
cd ~/BattleShip_Proj/project2-main/project2-main
./client -t < ~/BattleShip_Proj/project2-cases-main/project2-cases-main/<task>/<case>/client1-in.txt
```

**Run all visible tests at once:**
```bash
bash ~/BattleShip_Proj/project2-main/project2-main/run_tests.sh
```

---

## Task 1 — Connection Establishment (2 marks)

### Description

Establish TCP connections between clients and the server. The client sends a 4-byte `game_id` (network byte order) immediately after connect. The server accepts connections, assigns Player 1 / Player 2 by join order, and notifies both players when the game is ready.

### Implemented features

- Server: `engine_init`, port parsing, `SO_REUSEADDR`, `listen`/`accept`, `select()` event loop
- Client: `client_init`, `client_connect`, `client_wait_for_opponent`
- Protocol: `MSG_JOIN_OK`, `MSG_JOIN_FAIL`, `MSG_READY`

### Execution

| Case | Command |
|------|---------|
| connect1 | `./client -t < .../task1/connect1/client1-in.txt` |
| connect2 | `./client -t < .../task1/connect2/client1-in.txt` |
| ready | `./client -t < .../task1/ready/client1-in.txt` |

*(Start `./server 30023` in another terminal before each run, or use `run_tests.sh`.)*

### Results

| Case | What it tests | Result |
|------|---------------|--------|
| `connect1` | Single client connects and sends game_id | **PASS** |
| `connect2` | Two clients connect (partial handshake) | **PASS** |
| `ready` | Both players connect; `wait_for_opponent` succeeds | **PASS** |

**Sample output (`connect1`):**
```text
Client p1: Success e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
```

**Sample output (`ready`):**
```text
Client p1: Success e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
Client p2: Success e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
```

---

## Task 2 — Ship Placement (2 marks)

### Description

Both players place four ships on the board. The server validates placement via `engine_place_ships`. Each client receives their player number only after **both** players have placed ships successfully.

### Implemented features

- Client: `client_send_ships` sends `MSG_SHIPS`, blocks until `MSG_SHIPS_ACK`
- Server: player assignment (P1 first, P2 second), `engine_place_ships`, dual-ACK after both placements
- Protocol: `MSG_SHIPS`, `MSG_SHIPS_ACK`, `MSG_SHIPS_FAIL`

### Execution

| Case | Command |
|------|---------|
| ships1 | `./client -t < .../task2/ships1/client1-in.txt` |
| ships-reverse | `./client -t < .../task2/ships-reverse/client1-in.txt` |

### Results

| Case | What it tests | Result |
|------|---------------|--------|
| `ships1` | Standard ship placement for P1 and P2 | **PASS** |
| `ships-reverse` | Ship array order does not affect validation | **PASS** |

**Sample output (`ships1`):**
```text
Client p1: Success 254eb864e7f25eeed1a24b2a6139ee9bbd5ef6d7efa1899c6ba48dbc672c6568
Client p2: Success 650ffcde59e164c1d795cb3d52dc10082faf91ff53b4d10ff247ea7a8952d058
```

---

## Task 3 — Gameplay (3 marks)

### Description

Players alternate moves after ship placement. The server enforces turn order, calls `engine_take_turn`, and notifies the opponent of each move. Supports hit, miss, sunk, invalid moves, and win.

### Implemented features

- Client: `client_send_move`, `client_receive_move`, `client_close`
- Server: turn alternation, `engine_take_turn`, opponent notification via `MSG_OPP_MOVE`
- Game end: `engine_end_game` on win; `MSG_GAME_OVER` sent to both players

### Execution

| Case | Command |
|------|---------|
| turn1-half | `./client -t < .../task3/turn1-half/client1-in.txt` |
| turn1-full | `./client -t < .../task3/turn1-full/client1-in.txt` |
| turn2 | `./client -t < .../task3/turn2/client1-in.txt` |
| sunk2 | `./client -t < .../task3/sunk2/client1-in.txt` |
| again-partial | `./client -t < .../task3/again-partial/client1-in.txt` |
| win | `./client -t < .../task3/win/client1-in.txt` |

### Results

| Case | What it tests | Result |
|------|---------------|--------|
| `turn1-half` | First move (partial turn flow) | **PASS** |
| `turn1-full` | Full first turn + opponent receives move | **PASS** |
| `turn2` | Second turn alternation | **PASS** |
| `sunk2` | Ship sunk detection | **PASS** |
| `again-partial` | Multiple game sessions in one test | **PASS** |
| `win` | Complete game through to win | **PASS** |

**Sample output (`win` — full game):**
```text
Client p1: Success 6f8c400cc32557f07f8a0b30a3f382a83350757b19dfa7c946992a9152b65d1c
Client p2: Success 44619dacd0aa49b9bc9e53781e42343a75be79034f41d4aea4310d28894b5584
```

---

## Task 4 — Extended Move Encoding (2 marks)

### Description

Extended mode uses larger opaque payloads for move results. The client sends `MSG_MOVE_EXT`; the server calls `engine_take_turn_extended` and relays the length-prefixed payload unchanged. Game ID is sent as 4-byte big-endian on connect.

### Implemented features

- Client: `client_send_move_extended`, `client_free_extended_result`
- Server: extended turn relay, `engine_extract_turn_result` for opponent notification
- Protocol: `MSG_MOVE_EXT`, `MSG_MOVE_EXT_RESULT`

### Execution

| Case | Command | Server env |
|------|---------|------------|
| extended-simple | `./client -t < .../task4/extended-simple/client1-in.txt` | `ENGINE_MODE=extended_simple ./server 30023` |
| extended-long | `./client -t < .../task4/extended-long/client1-in.txt` | `ENGINE_MODE=extended_long ./server 30023` |
| extended-game-id | `./client -t < .../task4/extended-game-id/client1-in.txt` | `ENGINE_MODE=extended_long ./server 30023` |

*(Or use `run_tests.sh` which sets `ENGINE_MODE` automatically.)*

### Results

| Case | What it tests | Result |
|------|---------------|--------|
| `extended-simple` | Short extended payloads | **PASS** |
| `extended-long` | Large extended payloads | **PASS** |
| `extended-game-id` | Non-zero game_id on wire (`11189196`) | **PASS** |

**Sample output (`extended-simple`):**
```text
Client p1: Success e42457f1fc72fad59da5dfd1149debda0f3e6abda98925cfe831ab6562c02d5f
Client p2: Success 44619dacd0aa49b9bc9e53781e42343a75be79034f41d4aea4310d28894b5584
```

---

## Task 5 — Multiple Concurrent Games (2 marks)

### Description

The server handles multiple games at the same time using a single-threaded `select()` loop. Games are routed by `game_id`. A slow client in one game must not block another game. All `engine_*` calls are protected by a mutex.

### Implemented features

- `select()` on listen socket + all client FDs
- Per-game state map (up to 32 games, 64 clients)
- `pthread_mutex_t` around every engine call
- Literal string `MULTIPLE_GAMES` in `server_c/game.c` (marking requirement)

### Execution

| Case | Command |
|------|---------|
| m-ships | `./client -t < .../task5/m-ships/client1-in.txt` |
| m-win | `./client -t < .../task5/m-win/client1-in.txt` |

### Results

| Case | What it tests | Result |
|------|---------------|--------|
| `m-ships` | Game 1 completes ship placement while game 0 waits | **PASS** |
| `m-win` | Interleaved turns across two concurrent games | **PASS** |

**Sample output (`m-ships` — 4 clients, 2 games):**
```text
Client p1: Success 254eb864e7f25eeed1a24b2a6139ee9bbd5ef6d7efa1899c6ba48dbc672c6568
Client p2: Success 650ffcde59e164c1d795cb3d52dc10082faf91ff53b4d10ff247ea7a8952d058
Client p3: Success 254eb864e7f25eeed1a24b2a6139ee9bbd5ef6d7efa1899c6ba48dbc672c6568
Client p4: Success 650ffcde59e164c1d795cb3d52dc10082faf91ff53b4d10ff247ea7a8952d058
```

---

## Task 6 — Error Handling (2 marks)

### Description

Robust handling of disconnects, invalid connections, engine failures, and server startup errors with correct exit codes.

### Implemented features

| Scenario | Behaviour |
|----------|-----------|
| No port argument | Exit code `1` |
| `engine_init` fails (`ENGINE_MODE=fail_new`) | Exit code `2` |
| Bind/listen failure | Exit code `3` + error message |
| `engine_init_game` fails (`ENGINE_MODE=fail_init`) | `MSG_JOIN_FAIL` to client |
| Third client same game_id | `MSG_JOIN_FAIL` / connect returns false |
| Mid-game disconnect | End game, notify both, allow re-connect on same game_id |
| Malformed protocol data | Tear down game, notify players |
| Invalid move | Return `Invalid` without ending game |

### Execution

| Case | Command | Notes |
|------|---------|-------|
| disconnect-send | `./client -t < .../task6/disconnect-send/client1-in.txt` | P2 disconnects mid-game; P3/P4 re-join |
| disconnect-send-extended | `./client -t < .../task6/disconnect-send-extended/client1-in.txt` | Same, extended mode |
| connect-extra-1 | `./client -t < .../task6/connect-extra-1/client1-in.txt` | Third client rejected |
| fail-game-init | `./client -t < .../task6/fail-game-init/client1-in.txt` | `ENGINE_MODE=fail_init` |
| invalid port | `./server invalid` | Expect exit `1` |
| fail engine init | `ENGINE_MODE=fail_new ./server 30023` | Expect exit `2` |

### Results

| Case | What it tests | Result |
|------|---------------|--------|
| `disconnect-send` | Disconnect mid-game; new session on same game_id | **PASS** |
| `disconnect-send-extended` | Disconnect in extended mode | **PASS** |
| `connect-extra-1` | Reject third player on full game | **PASS** |
| `fail-game-init` | Engine game init failure | **PASS** |
| Invalid port | Server exit code 1 | **PASS** (`exit=1`) |
| `fail_new` engine | Server exit code 2 | **PASS** (`exit=2`) |

**Sample output (`disconnect-send`):**
```text
Client p1: L32 MoveErrorSelf
Client p2: Success 650ffcde59e164c1d795cb3d52dc10082faf91ff53b4d10ff247ea7a8952d058
Client p3: Success 254eb864e7f25eeed1a24b2a6139ee9bbd5ef6d7efa1899c6ba48dbc672c6568
Client p4: Success 650ffcde59e164c1d795cb3d52dc10082faf91ff53b4d10ff247ea7a8952d058
```

---

## Full automated test run

```bash
cd ~/BattleShip_Proj/project2-main/project2-main
bash run_tests.sh
```

### Results (latest run)

```text
=== task1/connect1 ===          PASS
=== task1/connect2 ===          PASS
=== task1/ready ===             PASS
=== task2/ships1 ===            PASS
=== task2/ships-reverse ===     PASS
=== task3/turn1-half ===        PASS
=== task3/turn1-full ===        PASS
=== task3/turn2 ===             PASS
=== task3/sunk2 ===             PASS
=== task3/again-partial ===     PASS
=== task3/win ===               PASS
=== task4/extended-simple ===   PASS
=== task4/extended-long ===     PASS
=== task4/extended-game-id ===  PASS
=== task5/m-ships ===           PASS
=== task5/m-win ===             PASS
=== task6/disconnect-send ===   PASS
=== task6/disconnect-send-extended === PASS
=== task6/connect-extra-1 ===   PASS
=== task6/fail-game-init ===    PASS
invalid port exit=1
fail_new exit=2
Overall FAIL=0
```

**Summary: 20/20 visible test cases passed.**

---

## Source layout

```text
project2-main/project2-main/
├── Makefile
├── run_tests.sh
├── common/
│   ├── netutil.c / netutil.h    # read_exact, write_exact, u32 BE I/O
│   └── protocol.c / protocol.h  # framed binary message protocol
├── client_c/
│   ├── client.c                 # full client.h implementation
│   └── client.h                 # provided interface (unchanged)
├── server_c/
│   ├── server.c                 # main(), select loop, accept
│   ├── game.c / game.h          # per-game state machine
│   └── engine.h                 # provided engine API (unchanged)
└── project2-bin/
    ├── libengine.a
    └── librunner.a
```

---

## Notes for the client

1. **Not a web application** — the server uses a custom TCP protocol. It cannot be opened in a browser (`http://IP:30023` will not work).
2. **Server idle behaviour** — after starting, the server prints one line and waits silently for connections. This is normal.
3. **Do not run `run_tests.sh` and manual `./server` at the same time** — both use port 30023.
4. **Free the port before manual server start:**
   ```bash
   fuser -k 30023/tcp 2>/dev/null
   ```
5. **Submission** — push to `feit-comp30023-2026/proj2-<username>` on GitHub; CI runs the same visible tests automatically.

---

*Document generated for client delivery — COMP30023 Project 2 Battleship C implementation.*
