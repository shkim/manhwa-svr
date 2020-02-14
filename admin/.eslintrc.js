module.exports = {
  root: true,
  env: {
    node: true,
  },
  extends: [
    'plugin:vue/essential',
    '@vue/airbnb',
  ],
  parserOptions: {
    parser: 'babel-eslint',
  },
  rules: {
    'no-console': process.env.NODE_ENV === 'production' ? 'error' : 'off',
    'no-debugger': process.env.NODE_ENV === 'production' ? 'error' : 'off',

    'max-len': 0,
    'arrow-parens': 0,
    'no-bitwise': 0,
    'no-param-reassign': ['warn', { 'props': false }],
    'no-unused-vars': ['warn', { "argsIgnorePattern": "ignored|res" }],
    'object-curly-newline': 0,
    'prefer-destructuring': 0,

    'vue/multiline-html-element-content-newline': 0,
    'vue/singleline-html-element-content-newline': 0,
    'vue/no-v-html': 0,
    'vue/html-indent': ['error', 2, {
      'ignores': ["VElement[name=script].children"]
    }],
    'vue/max-attributes-per-line': ["warn", {
      "singleline": 1,
      "multiline": {
        "max": 1,
        "allowFirstLine": false
      }
    }]
  },
  parserOptions: {
    parser: 'babel-eslint',
  },
};
