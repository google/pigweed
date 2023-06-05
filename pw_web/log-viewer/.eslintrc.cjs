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
    plugins: ["@typescript-eslint", "lit-a11y"],
    rules: {},
    ignorePatterns: ["src/legacy/**/*", "src/assets/**", "src/models/**"],
};
