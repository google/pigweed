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

// TODO: refactor this file and crate::riscv to separate generic elf code from
// riscv specific code.
#![allow(clippy::print_stdout)]

use core::fmt::Debug;
use std::collections::HashSet;
use std::path::Path;

use anyhow::{Context, anyhow};
use object::read::elf::{ElfFile32, FileHeader};
use object::{LittleEndian, elf};

use crate::riscv::call_graph::{FuncRepo, Function, list_functions};
use crate::riscv::{DecodedInstr, ElfMem, InstrA, Reg};

pub mod riscv;

/// Check to see if the elf-file at `elf_path` contains any calls to the
/// `panic_is_possible`` symbol, and if so, try to find the line numbers where
/// these potential panics originate from.
pub fn check_panic(elf_path: &Path) -> anyhow::Result<()> {
    const E: object::LittleEndian = object::LittleEndian;
    let ctx = || format!("In elf file {}", elf_path.display());
    let elf_bytes = std::fs::read(elf_path).with_context(ctx)?;
    let elf =
        ElfFile32::<object::LittleEndian, _>::parse(elf_bytes.as_slice()).with_context(ctx)?;
    let elf_mem = ElfMem::new(&elf, E)?;
    let funcs = list_functions(&elf, &elf_mem)?;
    let func_repo = FuncRepo::new(funcs).unwrap();
    if let Some(panic_func) = func_repo.get_func_by_symbol("panic_is_possible") {
        match elf.elf_header().e_machine(E) {
            elf::EM_ARM => solve_arm(&elf_mem, &func_repo, panic_func),
            elf::EM_RISCV => solve_riscv(&elf_mem, &func_repo, panic_func),
            _ => {
                return Err(anyhow!(
                    "Unsupported machine type: {:?}",
                    elf.elf_header().e_machine
                ));
            }
        }
    }
    if find_symbol_address(elf.elf_symbol_table(), "panic_is_possible").is_ok() {
        return Err(anyhow!(
            "File {} contains the symbol panic_is_possible. \n\
        This indicates that the optimizer was unable to optimize out all panics. \n\
        Please remove the offending panic call site.",
            elf_path.display()
        ));
    }
    Ok(())
}

fn solve_riscv(elf_mem: &ElfMem, func_repo: &FuncRepo, panic_func: &Function) {
    // Define a closure that the solver can use to read from .rodata and
    // .text when dereferencing pointers.
    let mem_read = |addr: u32| {
        elf_mem
            .get(addr, 4)
            .map(|a| u32::from_le_bytes(a.try_into().unwrap()))
    };
    // Solve for "all" possible (constant) values to the arguments to
    // panic_is_possible(filename: *const u8, filename_len: usize, line: u32, col: u32)
    let solutions = solve(
        func_repo,
        panic_func.start_addr(),
        // The RISC-V C calling convention stores the arguments starting at register a0-a3
        &[Reg::X10A0, Reg::X11A1, Reg::X12A2, Reg::X13A3],
        mem_read,
    );
    for solution in solutions {
        let (filename_ptr, filename_len, line, column) = (
            solution.results[0],
            solution.results[1],
            solution.results[2],
            solution.results[3],
        );
        // Lookup the string contents from .rodata
        let Some(filename) = elf_mem.get(filename_ptr, filename_len) else {
            println!("Couldn't find filename at addr {filename_ptr:x} len={filename_len}");
            continue;
        };
        let Ok(filename) = core::str::from_utf8(filename) else {
            continue;
        };
        println!();
        println!("Found panic {filename} line {line} column {column}. Branch trace:");
        for addr in solution.branch_trace {
            let Some((func, mut instr_iter)) = func_repo.instructions_at_addr(addr) else {
                continue;
            };
            let Some(instr) = instr_iter.next() else {
                continue;
            };
            println!(
                "  {: <36} ({})",
                instr.to_string(),
                rustc_demangle::demangle(func.symbol_name)
            );
        }
    }
}

fn solve_arm(_elf_mem: &ElfMem, _func_repo: &FuncRepo, _panic_func: &Function) {
    println!("Panic is possible.");
    println!("Location backtrace not supported on ARM.");
    // TODO: implement
}

/// Try to find all possible values of the registers specified in `regs` when
/// the PC is pointing at `addr`. The works well when `addr` is a function,
/// `regs` are the ABI arguments to that function, and the function is called
/// from multiple places in the code with different constants.
///
/// `mem_read_fn` is called when the solver needs to inspect .rodata at a
/// particular address. The argument is the address, and response should be the
/// word at that address.
fn solve<'a>(
    repo: &FuncRepo,
    addr: u32,
    regs: &[Reg],
    mem_read_fn: impl Fn(u32) -> Option<u32> + 'a,
) -> Vec<Solution> {
    let mut solver = Solver {
        repo,
        solutions: vec![],
        seen: HashSet::new(),
        depth: 0,
        mem_read_fn: Box::new(mem_read_fn),
    };
    // Set the start node in the jump linked-list to be the
    // provided address. This will be used to construct a "branch trace" (similar
    // to a stack trace but also include in-function branches) to give to the
    // user when the argument constants are solved.
    let jump = Jump::new(addr);
    let mut exprs: Vec<Expr> = regs.iter().map(|r| Expr::Reg(*r)).collect();
    solver.go(jump, &mut exprs);
    solver.solutions
}
struct Solver<'a> {
    repo: &'a FuncRepo<'a>,
    solutions: Vec<Solution>,
    // Seen instruction addresses, used to prevent us from getting stuck in a loop
    seen: HashSet<u32>,
    depth: usize,
    // Called to read from memory (typically .rodata).
    mem_read_fn: Box<dyn Fn(u32) -> Option<u32> + 'a>,
}
impl Solver<'_> {
    fn go(&mut self, jump: Jump, exprs: &mut [Expr]) {
        let addr = jump.addr;
        self.depth += 1;
        let Some((_func, mut iter)) = self.repo.instructions_at_addr(addr) else {
            self.depth -= 1;
            return;
        };
        // move forward one slot to include the jump instruction in prev()
        iter.next();
        // Iterate backwards over the instructions that led execution to this
        // point, building up an expression of operations used to construct the
        // register contents.
        while let Some(instr) = iter.prev() {
            if !self.seen.insert(instr.addr) {
                self.depth -= 1;
                // We've been here before, don't scan backwards any further.
                return;
            }
            let instr_d = instr.decode();
            if cfg!(feature = "solver_trace") {
                println!("{} {instr}", " ".repeat(self.depth));
            }
            let mut all_consts = true;
            for (expr_index, expr) in exprs.iter_mut().enumerate() {
                let old_expr = expr.clone();
                if let Err(err) = expr.handle_instr(instr, instr_d) {
                    if cfg!(feature = "solver_trace") {
                        println!("{} Aborting branch: {:?}", " ".repeat(self.depth), err);
                    }
                    self.depth -= 1;
                    return;
                }
                if cfg!(feature = "solver_trace") && *expr != old_expr {
                    println!("Expr index {expr_index} changed from {old_expr:?} to {expr:?}");
                }
                let found = matches!(expr, Expr::Const(_) | Expr::GlobalDeref(_));
                all_consts &= found;
            }
            if all_consts {
                // All of the register expressions have been resolved to a
                // constant or memory load from a constanst address; we've found a solution!
                let mut row = vec![];
                for expr in exprs {
                    row.push(match expr {
                        Expr::Const(v) => *v,
                        Expr::GlobalDeref(addr) => {
                            let Some(v) = (self.mem_read_fn)(*addr) else {
                                if cfg!(feature = "solver_trace") {
                                    println!("{} Aborting branch: could not read from address 0x{addr:x}", " ".repeat(self.depth));
                                }
                                self.depth -= 1;
                                return;
                            };
                            v
                        }
                        _ => unreachable!(),
                    });
                }
                self.solutions.push(Solution {
                    results: row,
                    branch_trace: jump.extend(instr.addr).branch_trace(),
                });
                self.depth -= 1;
                return;
            }
            for jumper_addr in self.repo.get_jumpers(instr.addr) {
                // The instruction at jumper_addr can jump to this instruction.
                // Recursively trace back through the code that can jump here.
                // Make an independent copy of the exprs so we can resume where
                // we left off once go() returns.
                let mut sub_exprs = exprs.to_vec();
                self.go(jump.extend(*jumper_addr), &mut sub_exprs);
            }
        }
        self.depth -= 1;
    }
}
#[derive(Debug)]
enum ExprErr {
    #[allow(dead_code)]
    RegCloberred(InstrA),
}
#[derive(Clone, Eq, PartialEq)]
enum Expr {
    Const(u32),
    GlobalDeref(u32),
    Reg(Reg),
    Add(Box<Expr>, Box<Expr>),
    PtrDeref(Box<Expr>),
}
impl Debug for Expr {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::Const(val) => {
                let val = (*val).cast_signed();
                if (-1024..1024).contains(&val) {
                    write!(f, "Const({val})")
                } else {
                    write!(f, "Const({val:08x})")
                }
            }
            Self::GlobalDeref(val) => write!(f, "GlobalDeref({val:08x})"),
            Self::Reg(val) => f.debug_tuple("Reg").field(val).finish(),
            Self::Add(a, b) => f.debug_tuple("Add").field(a).field(b).finish(),
            Self::PtrDeref(val) => f.debug_tuple("PtrDeref").field(val).finish(),
        }
    }
}
impl Expr {
    fn reg(reg: Reg) -> Self {
        match reg {
            Reg::X0 => Expr::Const(0),
            other => Expr::Reg(other),
        }
    }
    fn add(a: Expr, b: Expr) -> Expr {
        let mut result = Expr::Add(Box::new(a), Box::new(b));
        result.optimize();
        result
    }
    fn ptr_deref(ptr: Expr) -> Expr {
        let mut result = Expr::PtrDeref(Box::new(ptr));
        result.optimize();
        result
    }
    // Optimize/normalize the expression, folding constants, etc.
    fn optimize(&mut self) {
        match self {
            Expr::Add(a, b) => {
                if let (Expr::Const(a), Expr::Const(b)) = (&**a, &**b) {
                    *self = Expr::Const(a.wrapping_add(*b));
                    return;
                }
                if let Expr::Const(_) = &**a {
                    // Always put the constant last.
                    core::mem::swap(a, b);
                }
                if let Expr::Const(b) = &**b
                    && *b == 0
                {
                    *self = (**a).clone();
                    self.optimize();
                    return;
                }
                if let (Expr::Add(c, d), Expr::Const(b)) = (&**a, &**b)
                    && let Expr::Const(d) = &**d
                {
                    *self = Expr::add((**c).clone(), Expr::Const(b.wrapping_add(*d)));
                }
            }
            Expr::PtrDeref(ptr) => {
                if let Expr::Const(ptr) = &**ptr {
                    *self = Expr::GlobalDeref(*ptr);
                }
            }
            _ => {}
        }
    }
    fn handle_instr(&mut self, instr: InstrA, instr_d: DecodedInstr) -> Result<(), ExprErr> {
        match self {
            Self::Const(_) => {
                // Nothing to do; constants are constant
            }
            Self::GlobalDeref(_) => {}
            Self::Reg(reg) => {
                if instr_d.ty().rd() == Some(*reg) {
                    // This instruction changes the value of this register.
                    // Update the expression tree.
                    match instr_d {
                        // Load constant into this register
                        DecodedInstr::Auipc(_) => *self = Expr::Const(instr.absolute_imm()),
                        // Load constant into this register
                        DecodedInstr::Lui(i) => *self = Expr::Const(i.uimm()),
                        // Add two registers and put the result into this
                        // register. (Note: This can also be used to load a constant
                        // zero by making the source register x0; optimize()
                        // will collapse it to 0)
                        DecodedInstr::Add(i) => {
                            *self = Expr::add(Expr::reg(i.rs1()), Expr::reg(i.rs2()))
                        }
                        // Add another register + constant into this register.
                        // (this can also be used to load a constant by using x0
                        // as the source register; optimize() will collapse it
                        // to a constant expression)
                        DecodedInstr::Addi(i) => {
                            *self =
                                Expr::add(Expr::reg(i.rs1()), Expr::Const(i.imm().cast_unsigned()))
                        }
                        // Load from memory into this register.
                        DecodedInstr::Lw(i) => {
                            *self = Expr::ptr_deref(Expr::add(
                                Expr::reg(i.rs1()),
                                Expr::Const(i.imm().cast_unsigned()),
                            ))
                        }
                        _ => return Err(ExprErr::RegCloberred(instr)),
                    }
                }
            }
            Self::Add(a, b) => {
                // Forward the instruction down to both sides of the add
                // operation, in case it affects them.
                a.handle_instr(instr, instr_d)?;
                b.handle_instr(instr, instr_d)?;
                self.optimize();
            }
            Self::PtrDeref(ptr) => {
                // Forward the instruction down our pointer's address
                // expression, in case it affect it.
                ptr.handle_instr(instr, instr_d)?;
                self.optimize();
                // Self may have been changed by the call to optimize(), so make
                // sure we're still a PtrDeref to make the borrow checker happy.
                if let Self::PtrDeref(ptr) = self
                    && let DecodedInstr::Sw(i) = instr_d
                {
                    let store_expr =
                        Expr::add(Expr::reg(i.rs1()), Expr::Const(i.imm().cast_unsigned()));
                    if store_expr == **ptr {
                        // This instruction modifies the memory address we dereferenced.
                        // Update the expression tree.
                        *self = Expr::reg(i.rs2());
                        self.optimize();
                    }
                }
            }
        }
        Ok(())
    }
}
struct Jump<'a> {
    pub addr: u32,
    pub next: Option<&'a Jump<'a>>,
}
impl<'a> Jump<'a> {
    pub fn new(addr: u32) -> Self {
        Self { addr, next: None }
    }
    pub fn extend<'b: 'a>(&'b self, addr: u32) -> Jump<'b> {
        Self {
            addr,
            next: Some(self),
        }
    }
    pub fn branch_trace(&self) -> Vec<u32> {
        let mut result = vec![self.addr];
        let mut jump = self;
        while let Some(j) = jump.next {
            result.push(j.addr);
            jump = j;
        }
        result
    }
}
struct Solution {
    // The resolved constants, matching the indexes supplied in the `regs` slice
    // to `solve()` (for example, the solution to `regs[1]` will be in
    // `Solution::results[1]`)
    results: Vec<u32>,
    // The address of branch/jump instructions between where the solution was
    // found and the address provided to solve().
    branch_trace: Vec<u32>,
}

fn find_symbol_address(
    t: &object::read::elf::SymbolTable<object::elf::FileHeader32<LittleEndian>>,
    name: &str,
) -> anyhow::Result<u64> {
    const E: object::LittleEndian = object::LittleEndian;
    for sym in t.symbols() {
        if t.symbol_name(E, sym)? == name.as_bytes() {
            return Ok(sym.st_value.get(E).into());
        }
    }
    Err(anyhow!("Unable to find symbol {name:?}"))
}
