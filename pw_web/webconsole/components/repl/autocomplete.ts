// Copyright 2022 The Pigweed Authors
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

import {CompletionContext} from '@codemirror/autocomplete'
import {syntaxTree} from '@codemirror/language'

const completePropertyAfter = ['PropertyName', '.', '?.']
const dontCompleteIn = [
  'TemplateString',
  'LineComment',
  'BlockComment',
  'VariableDefinition',
  'PropertyDefinition'
]

export function completeFromGlobalScope(context: CompletionContext) {
  let nodeBefore = syntaxTree(context.state).resolveInner(context.pos, -1)

  if (
    completePropertyAfter.includes(nodeBefore.name) &&
    nodeBefore.parent?.name == 'MemberExpression'
  ) {
    let object = nodeBefore.parent.getChild('Expression')
    if (object?.name == 'VariableName') {
      let from = /\./.test(nodeBefore.name) ? nodeBefore.to : nodeBefore.from
      let variableName = context.state.sliceDoc(object.from, object.to)
      // @ts-ignore
      if (typeof window[variableName] == 'object') {
        // @ts-ignore
        return completeProperties(from, window[variableName])
      }
    }
  } else if (nodeBefore.name == 'VariableName') {
    return completeProperties(nodeBefore.from, window)
  } else if (context.explicit && !dontCompleteIn.includes(nodeBefore.name)) {
    return completeProperties(context.pos, window)
  }
  return null
}

function completeProperties(from: number, object: Object) {
  let options = []
  for (let name in object) {
    options.push({
      label: name,
      // @ts-ignore
      type: typeof object[name] == 'function' ? 'function' : 'variable'
    })
  }
  return {
    from,
    options,
    validFor: /^[\w$]*$/
  }
}
