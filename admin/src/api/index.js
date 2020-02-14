import auth from './auth';
import manhwa from './manhwa';
import { getAccessToken, setAccessToken, getAbsoluteApiUrl } from './base';

function getGqlErrorCode(err) {
  if (err.graphQLErrors) {
    for (let i = 0; i < err.graphQLErrors.length; i += 1) {
      if (err.graphQLErrors[i].code) return err.graphQLErrors[i].code;
    }
  }

  return undefined;
}

export default {
  ...auth,
  ...manhwa,
  getGqlErrorCode,
  getAccessToken,
  setAccessToken,
  getAbsoluteApiUrl,
};
