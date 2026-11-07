use std::fmt;

use crate::{Literal, LocalRw, RValue, RcLocal, Reduce, SideEffects, Traverse};

use super::{Binary, BinaryOperation};

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum UnaryOperation {
    Not,
    Negate,
    Length,
}

impl fmt::Display for UnaryOperation {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::Not => write!(f, "not "),
            Self::Negate => write!(f, "-"),
            Self::Length => write!(f, "#"),
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct Unary {
    pub value: Box<RValue>,
    pub operation: UnaryOperation,
}

impl SideEffects for Unary {
    fn has_side_effects(&self) -> bool {
        // `not` has no intrinsic side effects (it only inverts
        // truthiness). `negate` and `length` are also pure in
        // Luau (no metamethod calls at the AST level).
        // Side effects come only from the inner value.
        self.value.has_side_effects()
    }
}

impl Traverse for Unary {
    fn rvalues_mut(&mut self) -> Vec<&mut RValue> {
        vec![&mut self.value]
    }

    fn rvalues(&self) -> Vec<&RValue> {
        vec![&self.value]
    }
}

impl Reduce for Unary {
    fn reduce(self) -> RValue {
        // The clone here is needed because reduce_condition
        // takes ownership of the RValue. We compare the reduced
        // form to the original to determine if reduction is
        // possible (i.e., the expression simplifies).
        let does_reduce = |r: &RValue| &r.clone().reduce_condition() != r;

        fn is_boolean(r: &RValue) -> bool {
            match r {
                RValue::Binary(binary) if binary.operation.is_comparator() => true,
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::And | BinaryOperation::Or,
                }) => is_boolean(left) && is_boolean(right),
                RValue::Literal(Literal::Boolean(_)) => true,
                // no point matching strings, numbers and tables since reduce_condition has already been called
                _ => false,
            }
        }

        let ensure_boolean = |r| {
            if is_boolean(&r) {
                r
            } else {
                Binary::new(
                    Binary::new(r, Literal::Boolean(true).into(), BinaryOperation::And).into(),
                    Literal::Boolean(false).into(),
                    BinaryOperation::Or,
                )
                .into()
            }
        };

        match (self.value.reduce(), self.operation) {
            (RValue::Literal(Literal::Boolean(value)), UnaryOperation::Not) => {
                RValue::Literal(Literal::Boolean(!value))
            }
            (
                RValue::Unary(Unary {
                    value,
                    operation: UnaryOperation::Not,
                }),
                UnaryOperation::Not,
            ) => ensure_boolean(value.reduce_condition()),
            (RValue::Literal(Literal::Number(value)), UnaryOperation::Negate) => {
                RValue::Literal(Literal::Number(-value))
            }
            (RValue::Literal(Literal::String(value)), UnaryOperation::Length) => {
                // #"string" returns the byte length in Luau.
                // Unicode awareness depends on the Luau runtime;
                // the AST records the byte length which is what
                // the Luau # operator returns for string literals.
                RValue::Literal(Literal::Number(value.len() as f64))
            }
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::GreaterThan,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::LessThanOrEqual,
            }
            .reduce(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::LessThanOrEqual,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::GreaterThan,
            }
            .reduce(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::GreaterThanOrEqual,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::LessThan,
            }
            .reduce(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::LessThan,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::GreaterThanOrEqual,
            }
            .reduce(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::Equal,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::NotEqual,
            }
            .reduce(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::NotEqual,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::Equal,
            }
            .reduce(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation,
                }),
                UnaryOperation::Not,
            ) if (operation == BinaryOperation::And || operation == BinaryOperation::Or)
            // TODO: unnecessary clones
                && (does_reduce(&Unary {
                    value: left.clone(),
                    operation: UnaryOperation::Not,
                }.into()) || does_reduce(&Unary {
                    value: right.clone(),
                    operation: UnaryOperation::Not,
                }.into())) =>
            {
                ensure_boolean(
                    Binary {
                        left: Box::new(
                            Unary {
                                value: left,
                                operation: UnaryOperation::Not,
                            }
                            .reduce_condition(),
                        ),
                        right: Box::new(
                            Unary {
                                value: right,
                                operation: UnaryOperation::Not,
                            }
                            .reduce_condition(),
                        ),
                        operation: if operation == BinaryOperation::And {
                            BinaryOperation::Or
                        } else {
                            BinaryOperation::And
                        },
                    }
                    .reduce_condition(),
                )
            }
            (value, operation) => Self {
                value: Box::new(value),
                operation,
            }
            .into(),
        }
    }

    fn reduce_condition(self) -> RValue {
        // TODO: unnecessary clone
        let does_reduce = |r: &RValue| &r.clone().reduce_condition() != r;

        match (self.value.reduce_condition(), self.operation) {
            (RValue::Literal(Literal::Boolean(value)), UnaryOperation::Not) => {
                RValue::Literal(Literal::Boolean(!value))
            }
            (
                RValue::Unary(Unary {
                    value,
                    operation: UnaryOperation::Not,
                }),
                UnaryOperation::Not,
            ) => value.reduce_condition(),
            (RValue::Literal(Literal::Number(value)), UnaryOperation::Negate) => {
                RValue::Literal(Literal::Number(-value))
            }
            // __len has to return number, numbers are always truthy
            (_, UnaryOperation::Length) => RValue::Literal(Literal::Boolean(true)),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::GreaterThan,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::LessThanOrEqual,
            }
            .reduce_condition(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::LessThanOrEqual,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::GreaterThan,
            }
            .reduce_condition(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::GreaterThanOrEqual,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::LessThan,
            }
            .reduce_condition(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::LessThan,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::GreaterThanOrEqual,
            }
            .reduce_condition(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::Equal,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::NotEqual,
            }
            .reduce_condition(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation: BinaryOperation::NotEqual,
                }),
                UnaryOperation::Not,
            ) => Binary {
                left,
                right,
                operation: BinaryOperation::Equal,
            }
            .reduce_condition(),
            (
                RValue::Binary(Binary {
                    left,
                    right,
                    operation,
                }),
                UnaryOperation::Not,
            ) if (operation == BinaryOperation::And || operation == BinaryOperation::Or)
            // TODO: unnecessary clones
                && (does_reduce(&Unary {
                    value: left.clone(),
                    operation: UnaryOperation::Not,
                }.into()) || does_reduce(&Unary {
                    value: right.clone(),
                    operation: UnaryOperation::Not,
                }.into())) =>
            {
                Binary {
                    left: Box::new(
                        Unary {
                            value: left,
                            operation: UnaryOperation::Not,
                        }
                        .reduce_condition(),
                    ),
                    right: Box::new(
                        Unary {
                            value: right,
                            operation: UnaryOperation::Not,
                        }
                        .reduce_condition(),
                    ),
                    operation: if operation == BinaryOperation::And {
                        BinaryOperation::Or
                    } else {
                        BinaryOperation::And
                    },
                }
                .reduce_condition()
            }
            (value, operation) => Self {
                value: Box::new(value),
                operation,
            }
            .into(),
        }
    }
}

impl Unary {
    pub fn new(value: RValue, operation: UnaryOperation) -> Self {
        Self {
            value: Box::new(value),
            operation,
        }
    }

    pub fn precedence(&self) -> usize {
        7
    }

    pub fn group(&self) -> bool {
        (self.precedence() > self.value.precedence())
            || (matches!(self.operation, UnaryOperation::Negate)
                && (matches!(
                    *self.value,
                    RValue::Unary(Unary {
                        operation: UnaryOperation::Negate,
                        ..
                    })
                ) || matches!(
                    *self.value,
                    RValue::Literal(Literal::Number(value))
                        if value.is_finite() && value.is_sign_negative()
                )))
    }
}

impl LocalRw for Unary {
    fn values_read(&self) -> Vec<&RcLocal> {
        self.value.values_read()
    }

    fn values_read_mut(&mut self) -> Vec<&mut RcLocal> {
        self.value.values_read_mut()
    }
}

impl fmt::Display for Unary {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{}{}",
            self.operation,
            if self.group() {
                format!("({})", self.value)
            } else {
                format!("{}", self.value)
            }
        )
    }
}
