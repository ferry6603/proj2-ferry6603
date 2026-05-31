pub mod client_bindings;

extern crate core;
use crate::client_bindings::{MoveResult, Ship, TurnResult};

/// Data structure to hold client state.
pub struct ClientImplementation;

impl ClientImplementation {
    /// Initialise the client-side implementation.
    pub fn init() -> Option<Self> {
        Some(Self)
    }

    /// Connect to the game server.
    //
    /// * `addr` - Address or hostname of server.
    /// * `port` - Port number of server.
    /// * `game_id` - ID of the game to join.
    ///
    /// Returns true on successful connection, false otherwise.
    pub fn connect(&mut self, _addr: &str, _port: u16, _game_id: u32) -> bool {
        // Note: Remove the _ prefixes once you implement the functions.
        // They are currently used to suppress unused variable warnings.
        true
    }

    // Wait until 2 players have joined the game.
    //
    // Returns true when both players have joined, false on failure.
    pub fn wait_for_opponent(&mut self) -> bool {
        true
    }

    /// Send the player's ship placements to the server.
    /// This function should not return until both player's ship placements have been acknowledged by the server.
    ///
    /// * `ships` - Array of 4 ships to place for the current player.
    ///
    /// Returns player number of this instance (1 or 2) on success, or -1 on failure.
    pub fn send_ships(&mut self, _ships: &[Ship; 4]) -> i8 {
        -1
    }

    /// Send the player's move to the server.
    ///
    /// * `coordinate` - Coordinate of the move to perform.
    ///
    /// Returns result of the move.
    pub fn send_move(&mut self, _coordinate: &str) -> TurnResult {
        TurnResult::Invalid
    }

    /// Send the player's move to the server, with request for extended move encoding.
    ///
    /// * `coordinate` - Coordinate of the move to perform.
    ///
    /// Returns variable length data, which is parsed by the caller.
    pub fn send_move_extended(&mut self, _coordinate: &str) -> Option<Box<[u8]>> {
        None
    }

    //// Receive the opponent's move from the server.
    ////
    //// Returns coordinate and result of the opponent's move.
    pub fn receive_move(&mut self) -> MoveResult {
        MoveResult {
            coordinate: None,
            result: TurnResult::Invalid,
        }
    }

    /// Disconnect (must do) and clean up client state (if needed).
    ///
    /// Note: `ClientImplementation` will be dropped after this is called.
    pub fn close(&mut self) {}
}
