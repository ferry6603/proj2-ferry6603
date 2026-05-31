#![allow(clippy::missing_safety_doc)]

use core::fmt;
use std::ffi::{CStr, CString, c_char, c_void};

use crate::ClientImplementation;

/// The direction a ship extends towards.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Direction {
    Horizontal = 0,
    Vertical = 1,
}

impl fmt::Display for Direction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Direction::Horizontal => write!(f, "Horizontal"),
            Direction::Vertical => write!(f, "Vertical"),
        }
    }
}

/// Representation of a ship.
#[derive(Debug, PartialEq, Eq)]
pub struct Ship<'a> {
    /// Coordinate of ship's starting position (e.g. "A1").
    pub coordinate: &'a str,
    /// Length of ship in units (2, 3, 4, 5).
    pub length: u8,
    /// Direction of ship (Horizontal or Vertical).
    pub direction: Direction,
}

/// Outcome of a move.
#[repr(C)]
#[derive(Debug, PartialEq, Eq)]
pub enum TurnResult {
    Miss = 0,
    Hit = 1,
    Win = 2,
    Sunk2 = 4,
    Sunk3 = 5,
    Sunk4 = 6,
    Sunk5 = 7,
    Invalid = -1,
}

impl fmt::Display for TurnResult {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            TurnResult::Miss => write!(f, "Miss"),
            TurnResult::Hit => write!(f, "Hit"),
            TurnResult::Win => write!(f, "Win"),
            TurnResult::Sunk2 => write!(f, "Sunk ship of length 2"),
            TurnResult::Sunk3 => write!(f, "Sunk ship of length 3"),
            TurnResult::Sunk4 => write!(f, "Sunk ship of length 4"),
            TurnResult::Sunk5 => write!(f, "Sunk ship of length 5"),
            TurnResult::Invalid => write!(f, "Invalid move"),
        }
    }
}

// Represents the result from a move made by the opponent.
pub struct MoveResult {
    /// Coordinate of the opponent's move.
    pub coordinate: Option<String>,
    /// Outcome of the opponent's move.
    pub result: TurnResult,
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn client_init() -> *mut ClientImplementationPointer {
    let Some(implementation) = ClientImplementation::init() else {
        return std::ptr::null_mut();
    };

    Box::into_raw(Box::new(implementation)).cast::<ClientImplementationPointer>()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn client_connect(
    client: *mut ClientImplementationPointer,
    addr: *const c_char,
    port: u16,
    game_id: u32,
) -> bool {
    let Some(client) = (unsafe { from_ffi_ptr_mut(client) }) else {
        return false;
    };

    if addr.is_null() {
        return false;
    }

    // Convert address from C string to Rust string
    let addr_cstr = unsafe { CStr::from_ptr(addr) };
    let Ok(addr_str) = addr_cstr.to_str() else {
        return false;
    };

    client.connect(addr_str, port, game_id)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn client_wait_for_opponent(
    client: *mut ClientImplementationPointer,
) -> bool {
    let Some(client) = (unsafe { from_ffi_ptr_mut(client) }) else {
        return false;
    };

    client.wait_for_opponent()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn client_send_ships(
    client: *mut ClientImplementationPointer,
    ships: *const [CShip; 4],
) -> i8 {
    let Some(client) = (unsafe { from_ffi_ptr_mut(client) }) else {
        return -1;
    };

    if ships.is_null() {
        return -1;
    }
    let rust_ships: [Ship; 4] = std::array::from_fn(|i| {
        let ship = unsafe { &(*ships)[i] };

        let coordinate =
            coordinate_from_c_ptr(ship.coordinate).expect("Invalid coordinate from runner");
        Ship {
            coordinate,
            length: ship.length,
            direction: ship.direction,
        }
    });

    client.send_ships(&rust_ships)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn client_send_move(
    client: *mut ClientImplementationPointer,
    coordinate: *const c_char,
) -> TurnResult {
    let Some(client) = (unsafe { from_ffi_ptr_mut(client) }) else {
        return TurnResult::Invalid;
    };

    let coordinate = coordinate_from_c_ptr(coordinate).expect("Invalid coordinate from runner");
    client.send_move(coordinate)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn client_send_move_extended(
    client: *mut ClientImplementationPointer,
    coordinate: *const c_char,
) -> ExtendedTurnResult {
    let Some(client) = (unsafe { from_ffi_ptr_mut(client) }) else {
        return ExtendedTurnResult {
            length: 0,
            data: std::ptr::null(),
        };
    };

    let coordinate = coordinate_from_c_ptr(coordinate).expect("Invalid coordinate from runner");

    let data = client.send_move_extended(coordinate);
    let Some(data) = data else {
        return ExtendedTurnResult {
            length: 0,
            data: std::ptr::null(),
        };
    };

    if data.is_empty() || data.len() > u16::MAX as usize {
        return ExtendedTurnResult {
            length: 0,
            data: std::ptr::null(),
        };
    }

    let data_len = data.len() as u16;
    let data_ptr = data.as_ptr() as *const c_void;
    std::mem::forget(data);

    ExtendedTurnResult {
        length: data_len,
        data: data_ptr,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn client_free_extended_result(result: ExtendedTurnResult) {
    if result.data.is_null() {
        return;
    }

    unsafe {
        drop(Box::from_raw(std::ptr::slice_from_raw_parts_mut(
            result.data as *mut u8,
            result.length as usize,
        )))
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn client_receive_move(
    client: *mut ClientImplementationPointer,
) -> CMoveResult {
    let Some(client) = (unsafe { from_ffi_ptr_mut(client) }) else {
        return CMoveResult {
            coordinate: std::ptr::null(),
            result: TurnResult::Invalid,
        };
    };

    let move_result = client.receive_move();
    let Some(coordinate) = &move_result.coordinate else {
        return CMoveResult {
            coordinate: std::ptr::null(),
            result: move_result.result,
        };
    };

    let coordinate_cstring = convert_coordinate(coordinate).into_raw();
    CMoveResult {
        coordinate: coordinate_cstring,
        result: move_result.result,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn client_free_move_result(result: CMoveResult) {
    if result.coordinate.is_null() {
        return;
    }

    unsafe {
        _ = CString::from_raw(result.coordinate as *mut c_char);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn client_close(client: *mut ClientImplementationPointer) {
    if let Some(client) = unsafe { from_ffi_ptr_mut(client) } {
        client.close();
    }

    if client.is_null() {
        return;
    }

    unsafe {
        drop(Box::from_raw(client.cast::<ClientImplementation>()));
    }
}

unsafe fn from_ffi_ptr_mut<'a>(
    client: *mut ClientImplementationPointer,
) -> Option<&'a mut ClientImplementation> {
    if client.is_null() {
        return None;
    }

    unsafe { Some(&mut *client.cast::<ClientImplementation>()) }
}

type ClientImplementationPointer = c_void;

/// `MoveResult` as a C interface.
#[repr(C)]
pub struct CMoveResult {
    pub coordinate: *const c_char,
    pub result: TurnResult,
}

/// Extended result payload, from `engine_take_turn_extended`.
#[repr(C)]
pub struct ExtendedTurnResult {
    /// Length of data in bytes.
    pub length: u16,
    /// Pointer to data. This can be a NULL pointer.
    pub data: *const c_void,
}

/// Ship representation as a C interface.
#[repr(C)]
pub struct CShip {
    pub coordinate: *const c_char,
    pub length: u8,
    pub direction: Direction,
}

/// Convert a C string coordinate (e.g. "A1") to a Rust string coordinate.
fn coordinate_from_c_ptr<'a>(coordinate: *const c_char) -> Option<&'a str> {
    if coordinate.is_null() {
        return None;
    }

    let c_str = unsafe { CStr::from_ptr(coordinate as *const c_char) };
    c_str.to_str().ok()
}

/// Convert a Rust string coordinate (e.g. "A1") to a C string coordinate.
fn convert_coordinate(rust_str: &str) -> CString {
    CString::new(rust_str.replace('\0', "(NUL)"))
        .expect("Failed to convert internal coordinate to C string")
}
