// Copyright 2023 The Pigweed Authors
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

/**
# Filter language

This language allows us to filter logs based on a set of conditions. The language is designed to
map directly to UI chip elements.

## Examples

1. Show logs of severity info or higher for entries where the moniker is `core/ffx-laboratory:hello`
   or the message contains “hello”.

  ```
  (moniker:core/ffx-laboratory:hello | message:hello) severity:info
  ```

2. Show logs of severity error or higher, where the component url contains a package named hello,
   where the manifest is named hello or where the message contains “hello world” but where the
   message doesn’t contain “bye”.

  ```
  (package_name:hello | manifest:hello | message:"hello world") severity:error !message:"bye"
  ```

3. Show logs from both core/foo and core/bar

  ```
  moniker:core/foo|core/bar

  # which is the same as:
  moniker:core/foo|moniker:core/bar
  ```

4. Show logs where any field contains foo, bar or baz either in the message, url, moniker, etc.

  ```
  any:foo|bar|baz
  ```

## Grammar

```

<expression> := <conjunction> | <conjunction> <or> <expression>

<conjunction> := <primary expression> | <primary expression> <and> <conjunction>

<primary expression> := <not> <primary expression>
    | '(' <expression> ')'
    | <filter>

<filter> := <category> <core operator> <core operator values>
    | severity <operator> <severities>
    | <pid tid category> <core operator> <numbers>
    | <time category> <time operator> <time value>
    | <custom key> <operator> <custom key values>
    | <regex>
    | <string>

<core operator> := <contains> | <equality>
<operator> := <contains> | <equality> | <comparison>
<contains> := ':'
<equality> := '='
<comparison> :=  '>' | '<' | '>=' | '<='

<category> := moniker | tag | package_name | manifest | message | any
<pid tid category> := pid | tid
<time operator> := '>' | '<'
<time values> := <number> <time unit>
<time unit> := s | ms | min
<custom key values> := <numbers> | <boolean> | <regex> | <strings>
<core operator values> := <regex> | <strings>

<severity> := trace | debug | info | warn | error | fatal
<severities> := <severity> | <severity seq and> | <severity seq or>
<severity seq and> := <severity> '&' <severity seq and>
<severity seq or> := <severity> '|' <severity seq or>

<boolean> := true | false

<regex> := '/' <chars with spaces> '/'

<string> := <chars> | "<chars with spaces>"
<strings> := <string> | <string seq and> | <string seq or>
<string seq and> := <string> '&' <string seq and>
<string seq or> := <string> '|' <string seq or>

<chars> := any sequence of characters without space
<chars with spaces> := any sequence of characters with spaces

<number> := any sequence of numbers, including negative, exp, etc
<numbers>  := <number> | <number seq and> | <number seq or>
<number seq and> := <number> '&' <number seq and>
<number seq or> := <number> '|' <number seq or>

<or> := '|' | or
<not> := not | '!'
<and> := and | '&' | ε   # just writing a space will be interpreted as AND in the top level
```

The severities, logical operators, categories are case insensitive. When doing a `:` (contains)
operation, the value being matched against will be treated as case insensitive.

The following categories are supported:

1. Moniker: identifies the component that emitted the log.

2. Tag: a log entry might contain one or more tags in its metadata.

3. Package name: The name of the package present in the corresponding section of the url with
    which the component was launched. Supported operations:

4. Manifest: The name of the manifest present in the corresponding section of the url with which
    the component was launched.

5. Message: The log message.

6. Severity: The severity of the log.

7. Pid and Tid: the process and thread ids that emitted the log.

8. Custom key: A structured field contained in the payload of a structured log.

9. Any: Matches a value in any of the previous sections.

All categories support `=`(equals) and `:` (contains). The `:` operator will do substring search,
equality when dealing with numbers of booleans or a minimum severity operation when applied to
`severity`. Custom keys and severities also support `>`, `<`, `>=`, `<=`, `=`.
*/

{
  function buildExpression(args) {
    if (args.rhs.items.length === 1) {
      return new options.Filter({
        category: args.category.toLowerCase(),
        subCategory: args.subCategory,
        operator: args.operator,
        value: args.rhs.items[0]
      });
    }
    const items = args.rhs.items.map((value) => {
      return new options.Filter({
        category: args.category.toLowerCase(),
        subCategory: args.subCategory,
        operator: args.operator,
        value,
      })
    });
    switch (args.rhs.reducedExpression) {
      case 'or':
        return new options.OrExpression(items);
      case 'and':
        return new options.AndExpression(items);
    }
  }
}

Expression
  = head:ConjunctionExpression tail:(_ Or _ ConjunctionExpression)* {
    if (tail.length === 0) {
      return head;
    }
    let items = tail.reduce((result, element) => {
      return result.concat([element[3]]);
    }, [head]);
    return new options.OrExpression(items);
  }

ConjunctionExpression
  = head:PrimaryExpression tail:(_ (And _)? PrimaryExpression)* {
    if (tail.length === 0) {
      return head;
    }
    let items = tail.reduce((result, element) => {
      return result.concat(element[2]);
    }, [head]);
    return new options.AndExpression(items);
  }

PrimaryExpression
  = Not _ expression:PrimaryExpression {
    return new options.NotExpression(expression);
  }
  / "(" expression:Expression ")" { return expression; }
  / filter:Filter { return filter; }

Filter
  = "severity"i operator:Operator severities:Severities {
    return buildExpression({
      category: 'severity',
      subCategory: undefined,
      operator: operator,
      rhs: severities,
    });
  }
  / category:Category operator:CoreOperator strings:CoreOperatorValues {
    return buildExpression({
      category,
      subCategory: undefined,
      operator: operator,
      rhs: strings,
    });
  }
  / category:"time" operator:TimeOperator strings:TimeNumbers {
    return buildExpression({
      category,
      subCategory: undefined,
      operator: operator,
      rhs: strings,
    });
  }
  / category:PidTidCategory operator:CoreOperator numbers:Numbers {
    return buildExpression({
      category,
      subCategory: undefined,
      operator: operator,
      rhs: numbers,
    });
  }
  / customKey:CustomKey operator:Operator values:CustomKeyValues {
    return buildExpression({
      category: 'custom',
      subCategory: customKey,
      operator: operator,
      rhs: values,
    });
  }
  / regex:Regex {
    return buildExpression({
      category: 'any',
      subCategory: undefined,
      operator: 'contains',
      rhs: { items: [regex] },
    });
  }
  / !ReservedWord string:String {
    return buildExpression({
      category: 'any',
      subCategory: undefined,
      operator: 'contains',
      rhs: { items: [string] },
    });
  };

// NOTE: the returned types must match the ones in the Operator type in filter.ts.
CoreOperator
  = ":" { return 'contains'; }
  / "=" { return 'equal'; }

// NOTE: the returned types must match the ones in the Operator type in filter.ts.
Operator
  = ":" { return 'contains'; }
  / "=" { return 'equal'; }
  / ">=" { return 'greaterEq'; }
  / ">" { return 'greater'; }
  / "<=" { return 'lessEq'; }
  / "<" { return 'less'; }

// TimeOperator only supports > and <
TimeOperator
  = ">" { return 'greater'; }
  / "<" { return 'less'; }

// NOTE: the returned types must match the ones in the Category type in filter.ts.
Category
  = "any"i
  / "custom"i
  / "manifest"i
  / "message"i
  / "time"i
  / "moniker"i
  / "package-name"i
  / "tag"i;

PidTidCategory
  = "pid"i
  / "tid"i;

CustomKeyValues
  = Numbers
  / b:Boolean { return { items: [b] }; }
  / r:Regex { return { items: [r] }; }
  / Strings

CoreOperatorValues
  = r:Regex { return { items: [r] }; }
  / Strings

// NOTE: the returned types must match the ones in the Severity type in filter.ts.
Severity
  = "trace"i
  / "debug"i
  / "info"i
  / "warning"i { return "warn"; }
  / "warn"i
  / "error"i
  / "fatal"i

Boolean
  = "true"i { return true; }
  / "false"i { return false; }

Regex = "/" chars:RegexChar* "/" {
  return new RegExp(chars.join(""));
}

RegexChar
  = [^/\\]
  / "\\/" { return "/"; }
  / "\\\\" { return "\\"; }

Strings
  = head:String tail:(ShorthandedStringSequence)? {
    if (tail) {
      tail.items.unshift(head);
      return tail;
    }
    return { items: [head] };
  }

ShorthandedStringSequence
  = items:("|" String)+ {
    return {
      reducedExpression: 'or',
      items: items.map((item) => item[1]),
    };
  }
  / items:("&" String)+ {
    return {
      reducedExpression: 'and',
      items: items.map((item) => item[1]),
    };
  }

CustomKey = [^:\\|&\\(\\)><=" ]+ { return text(); }

Numbers
  = head:Number tail:(ShorthandedNumberSequence)? {
    if (tail) {
      tail.items.unshift(head);
      return tail;
    }
    return { items: [head] };
  }

TimeNumbers
  = head:TimeNumber {
    return { items: [head] };
  }

ShorthandedNumberSequence
  = items:("|" Number)+ {
    return {
      reducedExpression: 'or',
      items: items.map((item) => item[1]),
    };
  }
  / items:("&" Number)+ {
    return {
      reducedExpression: 'and',
      items: items.map((item) => item[1]),
    };
  }

Severities
  = head:Severity tail:(ShorthandedSeveritySequence)? {
    if (tail) {
      tail.items.unshift(head);
      return tail;
    }
    return { items: [head] };
  }

ShorthandedSeveritySequence
  = items:("|" Severity)+ {
    return {
      reducedExpression: 'or',
      items: items.map((item) => item[1]),
    };
  }
  / items:("&" Severity)+ {
    return {
      reducedExpression: 'and',
      items: items.map((item) => item[1]),
    };
  }

Number
  = [0-9]+ {
      return parseInt(text(), 10);
}

// Time number in nanoseconds.
TimeNumber
  = [0-9]+ "s" {
    return parseInt(text(), 10) * 1e9;
  }
  / [0-9]+ "min" {
    return parseInt(text(), 10) * 1e9 * 60;
  }
  / [0-9]+ "ms" {
    return parseInt(text(), 10) * 1e6;
  }

String
  = '"' chars:CharWithinQuotes* '"' {
    return chars.join('');
  }
  /  chars:CharNotInQuotes+ { return chars.join(''); }

CharWithinQuotes
  = [^"\\]
  / '\\"' { return '"'; }
  / "\\\\" { return "\\"; }

CharNotInQuotes
  = [^"\\|&\\(\\): ]
  / "\\" char:(
      '"'
    / '\\'
    / '('
    / ')'
    / ':'
    / ' '
    / '|'
    / '&'
  ) {
    return char;
  }

Not = "!" / "not"i
And = "&" / "and"i
Or = "|" / "or"i

ReservedWord
  = "and"i
  / "or"i
  / "not"i

// Whitespace
_ = [ \t\n\r]*
