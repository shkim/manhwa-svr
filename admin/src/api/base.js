import { ApolloClient } from 'apollo-client';
import { ApolloLink } from 'apollo-link';
import { createHttpLink } from 'apollo-link-http';
import { onError } from 'apollo-link-error';
import { InMemoryCache } from 'apollo-cache-inmemory';
import axios from 'axios';
import fetch from 'unfetch';

const hostUrl = process.env.VUE_APP_API_HOST_URL || document.location.origin;

const KEY_ATOKEN = 'atoken';
let accessToken = localStorage.getItem(KEY_ATOKEN);

function getAccessToken() {
  return accessToken;
}

function setAccessToken(token) {
  accessToken = token;
  console.log('setAT=', accessToken);
  if (token) {
    localStorage.setItem(KEY_ATOKEN, token);
  } else {
    localStorage.removeItem(KEY_ATOKEN);
  }
}

const apiHttp = axios.create({
  baseURL: `${hostUrl}/api`,
  timeout: 60000,
});

function getAbsoluteApiUrl(uri, addAtk) {
  let url = `${hostUrl}/api${uri}`;
  if (addAtk) {
    url += (uri.indexOf('?') < 0) ? '?' : '&';
    url += `auth=${accessToken}`;
  }
  return url;
}

class ApiError extends Error {
  constructor(code, msg, data) {
    super(msg);
    Error.captureStackTrace(this, ApiError);
    this.code = code;
    this.msg = msg || `API Error ${code}`;
    this.data = data;
    // console.error(`API Error=${code}: ${msg}`);
  }
}

apiHttp.interceptors.request.use(config => {
  // config.headers['Access-Control-Allow-Origin'] = '*';
  config.headers.Authorization = accessToken;
  return config;
}, err => {
  console.error('ApiReqErr:', err);
  Promise.reject(err);
});

apiHttp.interceptors.response.use(resp => {
  if (resp && resp.data) {
    const { data } = resp;
    if (data.code === 0) {
      return data.data;
    }

    return Promise.reject(new ApiError(data.code, data.msg));
  }

  console.error('invalid api resp:', resp);
  return Promise.reject(new ApiError(500, 'Invalid API response'));
}, err => {
  if (err && err.response) {
    return Promise.reject(new ApiError(err.response.status, err.response.data));
  }
  return Promise.reject(err);
});

const ErrCodes = Object.freeze({
  10: '입력된 파라메터가 올바르지 않습니다.',
  50: '서버 내부에서 에러가 발생하였습니다.',
});

//------------------------------------------------------------------------------
// GraphQL Apollo

const authMiddleware = new ApolloLink((operation, forward) => {
  if (accessToken) {
    operation.setContext({
      headers: { authorization: accessToken },
    });
  }
  return forward(operation);
});

const knownErrExp = /^(\S+):(.*)$/;
const errorLink = onError(({ graphQLErrors, networkError, response/* , operation, forward */ }) => {
  console.log('onError!', networkError);
  if (graphQLErrors) {
    for (let i = 0; i < graphQLErrors.length; i += 1) {
      const err = graphQLErrors[i];
      const m = err.message.match(knownErrExp);
      if (m) {
        // eslint-disable-next-line
        err.code = m[1];
        err.message = m[2];
      }
    }
  }

  if (networkError) {
    if (networkError.toString().indexOf('Failed to fetch') >= 0) {
      console.error('서버에 연결하지 못하였습니다.');
    } else {
      console.error(`Network error: ${networkError}`);
    }
    if (response) response.errors = null;
  }
});

const httpLink = createHttpLink({ uri: `${hostUrl}/graphql`, fetch });

const apolloClient = new ApolloClient({
  link: ApolloLink.from([
    authMiddleware,
    errorLink,
    httpLink,
  ]),
  cache: new InMemoryCache(),
  connectToDevTools: !!(process.env.VUE_APP_DEBUG),
  defaultOptions: {
    watchQuery: {
      fetchPolicy: 'network-only',
      errorPolicy: 'ignore',
    },
    query: {
      loadingKey: 'loading',
      fetchPolicy: 'network-only',
      errorPolicy: 'all',
    },
  },
});

export {
  getAccessToken,
  setAccessToken,
  apiHttp,
  apolloClient,
  getAbsoluteApiUrl,
  ApiError,
  ErrCodes,
};
