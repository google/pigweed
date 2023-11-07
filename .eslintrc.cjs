// ESLint configuration
module.exports = {
  env: {
    browser: true,
    es2021: true,
  },
  root: true,
  extends: [
    "eslint:recommended",
    "plugin:@typescript-eslint/recommended",
    "plugin:lit-a11y/recommended",
  ],
  overrides: [],
  parserOptions: {
    ecmaVersion: "latest",
    sourceType: "module",
  },
  plugins: [
    "@typescript-eslint",
    "lit-a11y",
  ],
  rules: {
    "@typescript-eslint/ban-ts-comment": "warn",
    "@typescript-eslint/no-explicit-any": "warn",
    "@typescript-eslint/no-unused-vars": "warn",
  },
  ignorePatterns: [
    "**/next.config.js",
    "bazel-bin",
    "bazel-out",
    "bazel-pigweed",
    "bazel-testlogs",
    "node-modules",
    "pw_ide/ts/pigweed-vscode/webpack.config.js",
    "pw_web/log-viewer/src/assets/**",
    "pw_web/log-viewer/src/legacy/**/*",
    "pw_web/log-viewer/src/models/**",
  ],
};
