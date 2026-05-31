#!/bin/bash
CASES=/home/asim/BattleShip_Proj/project2-cases-main/project2-cases-main
PROJ=/home/asim/BattleShip_Proj/project2-main/project2-main
cd "$PROJ"

fuser -k 30023/tcp 2>/dev/null || true
sleep 0.2

run_case() {
  local dir="$1"
  local engine_mode="${2:-}"
  echo "=== Testing $dir ==="
  if [ -n "$engine_mode" ]; then
    ENGINE_MODE="$engine_mode" ./server 30023 &
  else
    ./server 30023 &
  fi
  local spid=$!
  sleep 0.4
  if ! timeout 20 ./client -t < "$dir/client1-in.txt" > /tmp/client_out.txt 2>/tmp/client_err.txt; then
    echo "FAIL client timeout/error"
    cat /tmp/client_err.txt
    kill $spid 2>/dev/null || true
    wait $spid 2>/dev/null || true
    return 1
  fi
  kill $spid 2>/dev/null || true
  wait $spid 2>/dev/null || true
  sleep 0.2
  if [ -f "$dir/client1-out.txt" ]; then
    if ! diff -q "$dir/client1-out.txt" /tmp/client_out.txt >/dev/null; then
      echo "FAIL output mismatch"
      diff "$dir/client1-out.txt" /tmp/client_out.txt || true
      return 1
    fi
  fi
  echo "PASS"
}

FAIL=0
run_case "$CASES/task1/connect1" || FAIL=1
run_case "$CASES/task1/connect2" || FAIL=1
run_case "$CASES/task1/ready" || FAIL=1
run_case "$CASES/task2/ships1" || FAIL=1
run_case "$CASES/task2/ships-reverse" || FAIL=1
run_case "$CASES/task3/turn1-half" || FAIL=1
run_case "$CASES/task3/turn1-full" || FAIL=1
run_case "$CASES/task3/turn2" || FAIL=1
run_case "$CASES/task3/sunk2" || FAIL=1
run_case "$CASES/task3/again-partial" || FAIL=1
run_case "$CASES/task3/win" || FAIL=1
run_case "$CASES/task4/extended-simple" extended_simple || FAIL=1
run_case "$CASES/task4/extended-long" extended_long || FAIL=1
run_case "$CASES/task4/extended-game-id" extended_long || FAIL=1
run_case "$CASES/task5/m-ships" || FAIL=1
run_case "$CASES/task5/m-win" || FAIL=1
run_case "$CASES/task6/disconnect-send" || FAIL=1
run_case "$CASES/task6/disconnect-send-extended" extended_simple || FAIL=1
run_case "$CASES/task6/connect-extra-1" || FAIL=1
run_case "$CASES/task6/fail-game-init" fail_init || FAIL=1

./server invalid; echo "invalid port exit=$?"
ENGINE_MODE=fail_new ./server 30023; echo "fail_new exit=$?"

echo "Overall FAIL=$FAIL"

# Release port so a manual ./server 30023 works afterward
fuser -k 30023/tcp 2>/dev/null || true

exit $FAIL
