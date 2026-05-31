extern crate core;

mod engine_bindings;
use crate::engine_bindings::{Direction, Engine, Ship, TurnResult};

/// Example usage of engine.
/// Use this as a reference only.
fn main() {
    let mut engine = Engine::new().expect("Failed to initialise engine");

    assert!(engine.init_game(0));

    let ships_player1 = [
        Ship {
            coordinate: "A1".to_owned(),
            length: 5,
            direction: Direction::Horizontal,
        },
        Ship {
            coordinate: "A2".to_owned(),
            length: 4,
            direction: Direction::Vertical,
        },
        Ship {
            coordinate: "C2".to_owned(),
            length: 3,
            direction: Direction::Horizontal,
        },
        Ship {
            coordinate: "G1".to_owned(),
            length: 2,
            direction: Direction::Vertical,
        },
    ];
    let result = engine.place_ships(0, &ships_player1);
    assert!(result == 1);

    let ships_player2 = [
        Ship {
            coordinate: "B1".to_owned(),
            length: 5,
            direction: Direction::Horizontal,
        },
        Ship {
            coordinate: "B2".to_owned(),
            length: 4,
            direction: Direction::Vertical,
        },
        Ship {
            coordinate: "D2".to_owned(),
            length: 3,
            direction: Direction::Horizontal,
        },
        Ship {
            coordinate: "H1".to_owned(),
            length: 2,
            direction: Direction::Vertical,
        },
    ];
    let result2 = engine.place_ships(0, &ships_player2);
    assert!(result2 == 2);

    // Ships were already placed
    let result3 = engine.place_ships(0, &ships_player1);
    assert!(result3 == -1);

    assert!(engine.take_turn(0, 1, "B1") == TurnResult::Hit);
    assert!(engine.take_turn(0, 2, "J10") == TurnResult::Miss);
    assert!(engine.take_turn(0, 1, "C1") == TurnResult::Hit);
    assert!(engine.take_turn(0, 2, "J9") == TurnResult::Miss);
    assert!(engine.take_turn(0, 1, "D1") == TurnResult::Hit);
    assert!(engine.take_turn(0, 2, "J8") == TurnResult::Miss);
    assert!(engine.take_turn(0, 1, "E1") == TurnResult::Hit);
    assert!(engine.take_turn(0, 2, "J7") == TurnResult::Miss);
    assert!(engine.take_turn(0, 1, "F1") == TurnResult::Sunk5);
    assert!(engine.take_turn(0, 2, "J6") == TurnResult::Miss);

    // Repeat of previous coordinate
    assert!(engine.take_turn(0, 1, "F1") == TurnResult::Invalid);
    // Invalid coordinate
    assert!(engine.take_turn(0, 1, "COMP30023") == TurnResult::Invalid);
    assert!(engine.take_turn(0, 1, "") == TurnResult::Invalid);
    // Not player 2's turn
    assert!(engine.take_turn(0, 2, "J5") == TurnResult::Invalid);

    assert!(engine.take_turn(0, 1, "B2") == TurnResult::Hit);
    assert!(engine.take_turn(0, 2, "J5") == TurnResult::Miss);
    assert!(engine.take_turn(0, 1, "B3") == TurnResult::Hit);
    assert!(engine.take_turn(0, 2, "J4") == TurnResult::Miss);
    assert!(engine.take_turn(0, 1, "B4") == TurnResult::Hit);
    assert!(engine.take_turn(0, 2, "J3") == TurnResult::Miss);
    assert!(engine.take_turn(0, 1, "B5") == TurnResult::Sunk4);
    assert!(engine.take_turn(0, 2, "J2") == TurnResult::Miss);

    assert!(engine.take_turn(0, 1, "D2") == TurnResult::Hit);
    assert!(engine.take_turn(0, 2, "J1") == TurnResult::Miss);
    assert!(engine.take_turn(0, 1, "E2") == TurnResult::Hit);
    assert!(engine.take_turn(0, 2, "I10") == TurnResult::Miss);
    assert!(engine.take_turn(0, 1, "F2") == TurnResult::Sunk3);
    assert!(engine.take_turn(0, 2, "I9") == TurnResult::Miss);

    assert!(engine.take_turn(0, 1, "H1") == TurnResult::Hit);
    assert!(engine.take_turn(0, 2, "I8") == TurnResult::Miss);
    let final_result = engine.take_turn(0, 1, "H02");
    assert!(final_result == TurnResult::Win);
    assert!(final_result != TurnResult::Sunk2);

    let invalid_extended_result = engine.take_turn_extended(0, 1, "Z9");
    assert!(invalid_extended_result.is_empty());

    let turn_result = engine.extract_turn_result(&invalid_extended_result);
    assert!(turn_result == TurnResult::Invalid);

    engine.end_game(0);
}
