// Parser for Lingo literal values as printed by the decompiler, e.g.
//   ["Mddle Ramp": [#uvs: [[[0.1, 0.2], ...]], #lightmap: "001"], ...]
// The game stores its baked lightmap UVs as one huge literal returned by a
// movie script handler (getUVs_UV_Lightmap), so we read it back as data.

#[derive(Debug, Clone)]
pub enum Value {
    Num(f64),
    Str(String),
    Sym(String),
    List(Vec<Value>),
    Props(Vec<(Value, Value)>),
    Void,
}

impl Value {
    pub fn key_name(&self) -> Option<&str> {
        match self {
            Value::Str(s) | Value::Sym(s) => Some(s),
            _ => None,
        }
    }

    pub fn get(&self, key: &str) -> Option<&Value> {
        if let Value::Props(p) = self {
            p.iter()
                .find(|(k, _)| k.key_name().map_or(false, |n| n.eq_ignore_ascii_case(key)))
                .map(|(_, v)| v)
        } else {
            None
        }
    }

    pub fn as_list(&self) -> Option<&Vec<Value>> {
        if let Value::List(l) = self { Some(l) } else { None }
    }

    pub fn as_num(&self) -> Option<f64> {
        if let Value::Num(n) = self { Some(*n) } else { None }
    }

    pub fn as_str(&self) -> Option<&str> {
        match self {
            Value::Str(s) => Some(s),
            Value::Num(_) => None,
            _ => None,
        }
    }
}

struct Parser<'a> {
    s: &'a [u8],
    i: usize,
}

impl<'a> Parser<'a> {
    fn ws(&mut self) {
        while self.i < self.s.len() && (self.s[self.i] as char).is_whitespace() {
            self.i += 1;
        }
    }

    fn peek(&mut self) -> Option<u8> {
        self.ws();
        self.s.get(self.i).copied()
    }

    fn expect(&mut self, c: u8) -> Result<(), String> {
        if self.peek() == Some(c) {
            self.i += 1;
            Ok(())
        } else {
            Err(format!("expected '{}' at {}", c as char, self.i))
        }
    }

    fn value(&mut self) -> Result<Value, String> {
        match self.peek() {
            Some(b'[') => self.list(),
            Some(b'"') => {
                self.i += 1;
                let start = self.i;
                while self.i < self.s.len() && self.s[self.i] != b'"' {
                    self.i += 1;
                }
                let v = String::from_utf8_lossy(&self.s[start..self.i]).to_string();
                self.i += 1;
                Ok(Value::Str(v))
            }
            Some(b'#') => {
                self.i += 1;
                let start = self.i;
                while self.i < self.s.len() && (self.s[self.i].is_ascii_alphanumeric() || self.s[self.i] == b'_') {
                    self.i += 1;
                }
                Ok(Value::Sym(String::from_utf8_lossy(&self.s[start..self.i]).to_string()))
            }
            Some(c) if c == b'-' || c == b'.' || c.is_ascii_digit() => {
                let start = self.i;
                self.i += 1;
                while self.i < self.s.len()
                    && (self.s[self.i].is_ascii_digit() || matches!(self.s[self.i], b'.' | b'e' | b'E' | b'-' | b'+'))
                {
                    self.i += 1;
                }
                let t = std::str::from_utf8(&self.s[start..self.i]).unwrap();
                t.parse::<f64>().map(Value::Num).map_err(|e| format!("bad number '{t}': {e}"))
            }
            Some(_) => {
                // Bare words: VOID, TRUE, FALSE.
                let start = self.i;
                while self.i < self.s.len() && self.s[self.i].is_ascii_alphabetic() {
                    self.i += 1;
                }
                let w = String::from_utf8_lossy(&self.s[start..self.i]).to_lowercase();
                match w.as_str() {
                    "void" => Ok(Value::Void),
                    "true" => Ok(Value::Num(1.0)),
                    "false" => Ok(Value::Num(0.0)),
                    _ => Err(format!("unexpected token '{w}' at {start}")),
                }
            }
            None => Err("unexpected end of input".into()),
        }
    }

    fn list(&mut self) -> Result<Value, String> {
        self.expect(b'[')?;
        if self.peek() == Some(b']') {
            self.i += 1;
            return Ok(Value::List(vec![]));
        }
        if self.peek() == Some(b':') {
            self.i += 1;
            self.expect(b']')?;
            return Ok(Value::Props(vec![]));
        }
        let first = self.value()?;
        if self.peek() == Some(b':') {
            self.i += 1;
            let v = self.value()?;
            let mut props = vec![(first, v)];
            while self.peek() == Some(b',') {
                self.i += 1;
                let k = self.value()?;
                self.expect(b':')?;
                let v = self.value()?;
                props.push((k, v));
            }
            self.expect(b']')?;
            Ok(Value::Props(props))
        } else {
            let mut items = vec![first];
            while self.peek() == Some(b',') {
                self.i += 1;
                items.push(self.value()?);
            }
            self.expect(b']')?;
            Ok(Value::List(items))
        }
    }
}

pub fn parse(text: &str) -> Result<Value, String> {
    let mut p = Parser { s: text.as_bytes(), i: 0 };
    p.value()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_nested_proplists() {
        let v = parse(r#"["a b": [#uvs: [[[1.5, -2], [0, 0.25]]], #lightmap: "001"], "c": [:]]"#).unwrap();
        let a = v.get("a b").unwrap();
        assert_eq!(a.get("lightmap").unwrap().as_str(), Some("001"));
        let uv = &a.get("uvs").unwrap().as_list().unwrap()[0].as_list().unwrap()[0];
        assert_eq!(uv.as_list().unwrap()[1].as_num(), Some(-2.0));
    }
}
