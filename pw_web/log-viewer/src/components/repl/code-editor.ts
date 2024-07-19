// Copyright 2024 The Pigweed Authors
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

import { LitElement, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { basicSetup, EditorView } from 'codemirror';
import { keymap } from '@codemirror/view';
import { python } from '@codemirror/lang-python';
import { CompletionContext, autocompletion } from '@codemirror/autocomplete';
import { EditorState, Prec } from '@codemirror/state';
import { oneDark } from '@codemirror/theme-one-dark';
import { AutocompleteSuggestion } from '../../repl-kernel';
import { styles } from './code-editor.styles';
import { themeDark } from '../../themes/dark';
import { themeLight } from '../../themes/light';
import { insertNewlineAndIndent } from '@codemirror/commands';

type AutocompleteHandler = (
  code: string,
  cursor: number,
) => Promise<AutocompleteSuggestion[]>;
type EvalHandler = (code: string) => Promise<void>;

const LOCALSTORAGE_KEY = 'replHistory';
@customElement('code-editor')
export class CodeEditor extends LitElement {
  private view: EditorView;
  private history: string[] = this.getHistory();
  private currentHistoryItem = -1;
  static styles = [themeDark, themeLight, styles];

  @state()
  _enableRun = true;

  constructor(
    private initialCode: string,
    private getAutocompleteSuggestions: AutocompleteHandler,
    private onEvalHandler: EvalHandler,
  ) {
    super();
  }

  firstUpdated() {
    const editorContainer = this.shadowRoot!.querySelector('.editor')!;
    const startState = EditorState.create({
      doc: this.initialCode,
      extensions: [
        basicSetup,
        python(),
        autocompletion({
          override: [this.myCompletions.bind(this)],
        }),
        oneDark,
        Prec.highest(
          keymap.of([
            {
              key: 'Enter',
              shift: (view) => {
                insertNewlineAndIndent(view);
                return true;
              },
              run: () => {
                // Shift key is not pressed, evaluate
                this.handleEval();
                return true;
              },
            },
            {
              key: 'ArrowUp',
              run: (view) => {
                if (this.history.length === 0 || this.currentHistoryItem === 0)
                  return true;
                if (this.currentHistoryItem === -1)
                  this.currentHistoryItem = this.history.length;
                const state = view.state;
                const selection = state.selection.main;

                // Check if cursor is at the beginning of line 1
                if (
                  state.doc.lines > 1 &&
                  selection.from !== state.doc.line(1).from
                ) {
                  return false;
                }
                this.currentHistoryItem -= 1;
                view.dispatch({
                  changes: {
                    from: 0,
                    to: this.view.state.doc.length,
                    insert: this.history[this.currentHistoryItem],
                  },
                  selection: {
                    anchor: this.history[this.currentHistoryItem].length,
                    head: this.history[this.currentHistoryItem].length,
                  },
                });
                return true;
              },
            },
            {
              key: 'ArrowDown',
              run: (view) => {
                if (this.history.length === 0 || this.currentHistoryItem === -1)
                  return true;
                const state = view.state;
                const selection = state.selection.main;

                // Check if cursor is at the end of the last line
                if (
                  state.doc.lines > 1 &&
                  selection.to !== state.doc.line(state.doc.lines).to
                ) {
                  return false;
                }
                this.currentHistoryItem += 1;
                if (this.currentHistoryItem === this.history.length) {
                  this.currentHistoryItem = -1;
                  view.dispatch({
                    changes: {
                      from: 0,
                      to: this.view.state.doc.length,
                      insert: '',
                    },
                  });
                  return true;
                }

                view.dispatch({
                  changes: {
                    from: 0,
                    to: this.view.state.doc.length,
                    insert: this.history[this.currentHistoryItem],
                  },
                  selection: {
                    anchor: this.history[this.currentHistoryItem].length,
                    head: this.history[this.currentHistoryItem].length,
                  },
                });

                return true;
              },
            },
          ]),
        ),
        EditorView.updateListener.of((update) => {
          if (update.docChanged) {
            const isEmpty = this.view.state.doc.length === 0;
            if (isEmpty) {
              this._enableRun = true;
              this.currentHistoryItem = -1;
            } else {
              this._enableRun = false;
            }
          }
        }),
      ],
    });

    this.view = new EditorView({
      state: startState,
      parent: editorContainer,
    });
  }

  private handleEval() {
    const code = this.view.state.doc.toString().trim();
    if (!code) return;
    this.onEvalHandler(code);
    this.addToHistory(code);
    this.view.dispatch({
      changes: { from: 0, to: this.view.state.doc.length, insert: '' },
    });
    this.currentHistoryItem = -1;
  }

  private getHistory(): string[] {
    let historyStore = [];
    if (localStorage.getItem(LOCALSTORAGE_KEY)) {
      try {
        historyStore = JSON.parse(localStorage.getItem(LOCALSTORAGE_KEY)!);
      } catch (e) {
        console.error('Repl history failed to parse.');
      }
    }
    return historyStore;
  }

  private addToHistory(command: string) {
    this.history.push(command);
    const historyStore = this.getHistory();
    historyStore.push(command);
    localStorage.setItem(LOCALSTORAGE_KEY, JSON.stringify(historyStore));
  }

  async myCompletions(context: CompletionContext) {
    const word = context.matchBefore(/\w*/);
    const cursor = context.pos;
    const code = context.state.doc.toString();

    try {
      const suggestions = await this.getAutocompleteSuggestions(code, cursor)!;
      return {
        from: word?.from || 0,
        options: suggestions.map((suggestion) => ({
          label: suggestion.text,
          type: suggestion.type || 'text',
        })),
      };
    } catch (error) {
      console.error('Error fetching autocomplete suggestions:', error);
      return null;
    }
  }

  render() {
    return html` <div class="editor-and-run">
      <div class="editor"></div>
      <md-icon-button ?disabled=${this._enableRun} @click=${this.handleEval}>
        <md-icon>&#xe037;</md-icon>
      </md-icon-button>
    </div>`;
  }
}
