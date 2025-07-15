// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

//! # pw_kernel User Space API
//!
//! ## Core Concepts
//!
//! ### Objects
//! Objects are the basic building block of functionality exposed to user space.
//! They are polymorphic and may be one of a limited set of types:
//!
//! - [Channel](#channel)
//! - [Wait Group](#wait-group)
//!
//! ### Handles
//! All system calls reference objects through a u32 handle which indexes into
//! a process-local handle table.
//!
//! ### Signals and Waiting
//! Every kernel object has a set of signals that can be pending and waited
//! upon.  The exact meaning and semantics of each signal vary between kernel
//! objects.  The signal types are:
//! - [`SIGNAL_READABLE`]: Object is readable.
//! - [`SIGNAL_WRITABLE`]: Object is writable.
//! - [`SIGNAL_ERROR`]: Object is in an error state.
//! - [`SIGNAL_USER`]: User defined signal.  Useful for out of band signaling
//!   between peers on a [Channel](#channel).
//!
//! Any kernel object can be waited for signals to assert using the
//! [`object_wait()`] syscall.  Multiple objects can be waited on simultaneously
//! using a [Wait Group](#wait-group) object.
//!
//! #### Open Questions
//! - What are the multi-threaded semantics of waiting.
//!
//! ### Object and Handle Creation
//! Initially only statically defined and allocated objects and handles are
//! supported.  The creation of these will be driven through build time
//! configuration and the necessary code will be generated to allocate kernel
//! data structures as well as expose handle definitions to user space
//! processes.
//!
//! ## Object Types
//!
//! ### Channel
//! A channel is a unidirectional connection between two asymmetric peers: an
//! initiator and a handler.  A channel allows the initiator peer to send a
//! buffer of data to the handler peer and wait for it's response.  A channel
//! can have a maximum of one transaction pending at a time and is designed to
//! not require intermediate kernel buffers.
//!
//! Both a synchronous and asynchronous API is offered to the initiator
//! while the handler side API is strictly non-blocking.  All data copies
//! between peers happen during the system call.
//!
//! The flow of a transaction is as follows:
//! - The initiator starts the transaction by providing send and receive
//!   buffers to one of the two transact system calls ([`channel_transact()`] or
//!   [`channel_async_transact()`]). This has the additional side effect of
//!   clearing [`SIGNAL_READABLE`] and [`SIGNAL_WRITABLE`] on the initiator.
//! - The handler's [`SIGNAL_READABLE`] will become asserted.
//! - The handler can now read the message in multiple calls to
//!   [`channel_read()`] causing the kernel to copy the data from the
//!   initiator's send buffer to the buffer provided to [`channel_read()`].
//! - The handler completes the transaction by calling [`channel_respond()`]
//!   and providing a response.  The kernel will immediately copy the response
//!   from the handler's buffer to the initiator's receive buffer.
//!   There is no built in mechanism for the handler to signal an error
//!   to the initiator.  This is left to the higher level protocol used to
//!   communicate over the channel.  This will clear [`SIGNAL_READABLE`] and
//!   [`SIGNAL_WRITABLE`] on the handler and raise [`SIGNAL_READABLE`] on
//!   the initiator.
//!
//! The handler's only ways of communicating with the initiator are by
//! - responding to an initiated transaction
//! - raising [`SIGNAL_USER`] on the initiator by calling
//!   [`object_raise_peer_user_signal()`]
//!
//! #### Initiator Signals
//! - [`SIGNAL_WRITABLE`] indicates there is no pending transaction and one
//!   can be started. Cleared on transaction initiation.
//! - [`SIGNAL_READABLE`] indicates the handler has responded to the
//!   pending transaction.  Cleared on transaction initiation.
//! - [`SIGNAL_ERROR`] indicates pending transaction has an error.  Cleared
//!   when the initiator is waited on.
//! - [`SIGNAL_USER`] indicates the handler calls [`object_raise_peer_user_signal()`].
//!   Cleared when the initiator is waited on.
//!
//! #### Handler Signals
//! - [`SIGNAL_READABLE`] indicates there is a pending transaction.  Cleared when
//!   the handler calls [`channel_respond()`].
//! - [`SIGNAL_WRITABLE`] indicates there is a pending transaction.  Cleared when
//!   the handler calls [`channel_respond()`].
//! - [`SIGNAL_ERROR`] indicates a pending transaction error.  No error states
//!   are defined at the moment.  In the future an error may be raised when
//!   the remote peer closes.
//! - [`SIGNAL_USER`] indicates the initiator calls [`object_raise_peer_user_signal()`].
//!   Cleared when the initiator is waited on.
//!
//! ### Wait Group
//! Wait groups provide a mechanism for waiting on multiple handles at once.
//! Handles can be added to and removed from a wait group with [`wait_group_add()`]
//! and [`wait_group_remove()`].  In addition to a set of signals to wait on,
//! an arbitrary `user_data` is provided to [`wait_group_add()`].  This value
//! is returned by [`object_wait()`], unmodified by the kernel, when the wait group is
//! waited on.
//!
//! #### Open questions:
//! - How is the wait group's member list allocated in there kernel.  To support
//!   a fully statically allocated kernel one of two approaches are being
//!   considered:
//!   - an object (or possibly handle) may only be in a single wait group at
//!     one time.  This allows a wait group to maintain an intrusive list of
//!     objects with the list element storage being stored in the object.
//!   - wait queues are statically sized at compile time and adding more objects
//!     than there is space for will return an error.
//!
//! ### Interrupt
//! In design
//!
//! ### Futex
//! In design
//!
//! ## System Calls
//! The C ABI system calls listed here are not intended to be called directly
//! by user space code and instead be accessed through language idiomatic
//! wrapper libraries.
//!
//! ### Generic Syscalls
//! - [`object_wait()`]
//! - [`object_raise_peer_user_signal()`]
//!
//! ### Channel Initiator Syscalls
//! - [`channel_transact()`]
//! - [`channel_async_transact()`]
//! - [`channel_async_cancel()`]
//!
//! ### Channel Handler Syscalls
//! - [`channel_read()`]
//! - [`channel_respond()`]
//!
//! ### Wait Group Syscalls
//! - [`wait_group_add()`]
//! - [`wait_group_remove()`]

#![no_std]
use pw_status::{Error, Result};

pub struct SysCallReturnValue(pub i64);

impl SysCallReturnValue {
    pub fn to_result_unit(self) -> Result<()> {
        let value = self.0;
        if value < 0 {
            // TODO debug assert if error number is out of range
            let value = (-value).cast_unsigned();
            // TODO(421404517): Avoid the lossy cast
            #[allow(clippy::cast_possible_truncation)]
            Err(unsafe { core::mem::transmute::<u32, Error>(value as u32) })
        } else {
            Ok(())
        }
    }
    pub fn to_result_u32(self) -> Result<u32> {
        let value = self.0;
        if value < 0 {
            // TODO debug assert if error number is out of range
            let value = (-value).cast_unsigned();
            // TODO(421404517): Avoid the lossy cast
            #[allow(clippy::cast_possible_truncation)]
            Err(unsafe { core::mem::transmute::<u32, Error>(value as u32) })
        } else {
            // TODO(421404517): Avoid the lossy cast
            #[allow(clippy::cast_possible_truncation)]
            Ok(value.cast_unsigned() as u32)
        }
    }
}

impl From<Result<u64>> for SysCallReturnValue {
    fn from(value: Result<u64>) -> Self {
        match value {
            // TODO - konkers: Debug assert on high bit of value being set.
            Ok(val) => Self(val.cast_signed()),
            Err(error) => Self(-(error as i64)),
        }
    }
}

#[repr(u16)]
pub enum SysCallId {
    // System calls prefixed with 0xF000 are reserved development/debugging use.
    DebugNoOp = 0xf000,
    DebugAdd = 0xf001,
    DebugPutc = 0xf002,
}

impl TryFrom<u16> for SysCallId {
    type Error = Error;

    fn try_from(value: u16) -> core::result::Result<Self, Error> {
        match value {
            // Safety: match
            0xf000..=0xf002 => Ok(unsafe { core::mem::transmute::<u16, SysCallId>(value) }),
            _ => Err(Error::InvalidArgument),
        }
    }
}

/// A mask of objects signals
pub type SignalMask = u32;

/// Bit in a [`SignalMask`] indicating that a object is readable.
pub const SIGNAL_READABLE: SignalMask = 1 << 0;

/// Bit in a [`SignalMask`] indicating that a object is writable.
pub const SIGNAL_WRITABLE: SignalMask = 1 << 1;

/// Bit in a [`SignalMask`] indicating that a object is in an error state.
pub const SIGNAL_ERROR: SignalMask = 1 << 2;

/// Bit in a [`SignalMask`] that has protocol specific meaning.
pub const SIGNAL_USER: SignalMask = 1 << 16;

/// Return value from the [`object_wait()`] syscall.
///
/// TODO: This is a bit heavy.  Define exact syscall ABI for this data.
#[repr(C)]
pub struct WaitReturn {
    /// Status of the [`object_wait()`] syscall.
    pub status: isize,

    /// Signals pending on this object.
    pub pending_signals: SignalMask,

    /// `user_data` of the wait group member.
    pub wait_group_user_data: usize,

    /// Signals pending on the wait group member.
    pub wait_group_pending_signals: SignalMask,
}

extern "C" {
    /// Perform a synchronous channel transaction
    ///
    /// Performs a transaction from the initiator side of a channel and blocks
    /// until the handler side has completed the transaction.  `send_data` and
    /// `recv_data` may overlap or be the same buffer.
    ///
    /// While non-block and infinite blocking semantics are not explicitly
    /// supported, they can be effectively achieved with:
    /// - Non-blocking: `deadline` == 0
    /// - Infinite blocking: `deadline` == `u64::MAX`
    ///
    /// This call will cause `SIGNAL_READABLE` to be cleared on the initiator
    /// channel object at the beginning of execution.  However by the time it
    /// returns without error, `SIGNAL_READABLE` will be set again.
    ///
    /// The maximum size of buffers that may be passed is `isize::MAX`.
    ///
    /// # Returns
    /// - `>=0`: Number of bytes received from the handler side.
    /// - [`Error::InvalidArgument`]: `object_handle` is not a valid initiator
    ///   channel object.
    /// - [`Error::ResourceExhausted`]: The channel already has a pending transaction.
    /// - [`Error::PermissionDenied`]: `send_data` or `recv_data` do not reference
    ///   valid memory regions in this processes' address space.
    /// - [`Error::DeadlineExceeded`]: The handler side did not respond before
    ///   `deadline` was exceeded.
    pub fn channel_transact(
        object_handle: u32,
        send_data: *mut u8,
        send_len: usize,
        recv_data: *mut u8,
        recv_len: usize,
        deadline: u64,
    ) -> isize;

    /// Perform an asynchronous channel transaction
    ///
    /// Initiates a transaction from the initiator side of a channel.
    /// send_data` must remain valid and readable and `recv_data` must remain
    /// valid and writeable for the duration of the transaction (completed or
    /// canceled.)  `send_data` and `recv_data` may overlap or be the same
    /// buffer.
    ///
    /// This call will cause `SIGNAL_READABLE` to be cleared on the initiator
    /// channel object.  It will be signaled by the handler side when it
    /// responds.
    ///
    /// # Returns
    /// - `0`: Transaction was successfully initiated.
    /// - [`Error::InvalidArgument`]: `object_handle` is not a valid initiator
    ///   channel object.
    /// - [`Error::ResourceExhausted`]: The channel already has a pending transaction.
    /// - [`Error::PermissionDenied`]: `send_data` or `recv_data` do not reference
    ///   valid memory regions in this processes' address space.
    pub fn channel_async_transact(
        object_handle: u32,
        send_data: *const u8,
        send_len: usize,
        recv_data: *mut u8,
        recv_len: usize,
    ) -> isize;

    /// Cancels a pending transaction on a channel
    ///
    /// # Returns
    /// - `0`: Pending transaction was successfully canceled.
    /// - [`Error::InvalidArgument`]: `object_handle` is not a valid initiator
    ///   channel object.
    /// - [`Error::FailedPrecondition`]: No transaction was pending on the channel.
    pub fn channel_async_cancel(object_handle: u32) -> isize;

    /// Perform a non-blocking read from a pending transaction.
    ///
    /// Attempts to read up to `buf_len` bytes from the `send_buffer` of the
    /// pending transaction starting from `offset`.  The kernel will copy the
    /// data from the initiators `send_buffer` into `buffer` before returning.
    ///
    /// The maximum size of buffer that may be passed is `isize::MAX`.
    ///
    /// # Returns
    /// - `>=0`: Number of bytes read from the send_buffer.
    /// - [`Error::InvalidArgument`]: `object_handle` is not a valid handler
    ///   channel object.
    /// - [`Error::OutOfRange`]: A read was requested outside the bound of the
    ///   initiator's `send_buffer`.
    /// - [`Error::FailedPrecondition`]: No transaction was pending on the channel.
    ///   This can happen in the middle of handling a transaction if the initiator
    ///   cancels the transaction.
    /// - [`Error::PermissionDenied`]: `buffer` does not reference a valid memory
    ///   region in this processes' address space.
    /// - TODO: What error should be returned if the initiator's `send_buffer`
    ///   is invalid.  Is that also permission denied?
    pub fn channel_read(
        object_handle: u32,
        offset: usize,
        buffer: *mut u8,
        buffer_len: usize,
    ) -> isize;

    /// Respond to and complete a pending transaction
    ///
    /// Causes the kernel to copy `buffer` into the initiator's `recv_buffer`
    /// and set `SIGNAL_READABLE` on the initiator channel object.
    ///
    /// The maximum size of buffer that may be passed is `isize::MAX`.
    pub fn channel_respond(object_handle: u32, buffer: *mut u8, buffer_len: usize) -> isize;

    /// Raise `SIGNAL_USER` on a paired object's peer
    ///
    /// Since channels are unidirectional, this serves as a way for the handler
    /// to signal the initiator.
    pub fn object_raise_peer_user_signal(object_handle: u32) -> isize;

    //
    // waiting
    //
    // Need to define multi threaded semantics

    /// Wait on a single object
    ///
    /// Waits for one of the signals in `signal_mask` to be pending on `object_handle`.
    ///
    /// # Returns
    /// - `WaitReturn.status >= 0`: Returns object specific metadata
    /// - [`Error::InvalidArgument`]: `object_handle` is not a valid object.
    /// - [`Error::DeadlineExceeded`]: The handler side did not respond before
    ///   `deadline` was exceeded.
    pub fn object_wait(object_handle: u32, signal_mask: SignalMask, deadline: u64) -> WaitReturn;

    /// Adds an object to a wait group
    ///
    /// Add `object` to `wait_group`.  `wait_group` will signal when one of the
    /// signals in `signal_mask` is raised on `object`.  `user_data` is passed,
    /// untouched by the kernel to the return value of `wait()`.
    ///
    /// # Returns
    /// `0`: Success
    /// - [`Error::InvalidArgument`]: `wait_group` is not a valid wait group or
    ///   `object` is not a valid object.
    /// - [`Error::ResourceExhausted`]: `object` is already in a wait group.
    pub fn wait_group_add(
        wait_group: u32,
        object: u32,
        signal_mask: SignalMask,
        user_data: usize,
    ) -> isize;

    /// Removes an object from a wait group
    ///
    /// # Returns
    /// `0`: Success
    /// - [`Error::InvalidArgument`]: `wait_group` is not a valid wait group or
    ///   `object` is not a valid object.
    /// - [`Error::NotFound`]: `object` is not in `wait_group`.
    pub fn wait_group_remove(wait_group: u32, object: u32) -> isize;

    //
    // Interrupts
    //
    // Signal only, latchable object signalable from "interrupt context"

}

pub trait SysCallInterface {
    fn debug_noop() -> Result<()>;
    fn debug_add(a: u32, b: u32) -> Result<u32>;
    fn debug_putc(a: u32) -> Result<u32>;
}
