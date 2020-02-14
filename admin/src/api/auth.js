import gql from 'graphql-tag';
import { apolloClient } from './base';

function login(userid, passwd) {
  return apolloClient.mutate({
    mutation: gql`
mutation login($userid: String!, $passwd: String!) {
  login(userid: $userid, passwd: $passwd) {
    accessToken, refreshToken
  }
}`,
    variables: {
      userid, passwd,
    },
  });
}

function logout(rtoken) {
  return apolloClient.mutate({
    mutation: gql`
mutation logout($rtoken: String!) {
  logout(rtoken: $rtoken)
}`,
    variables: {
      rtoken,
    },
  });
}

function refreshAuth(rtoken) {
  return apolloClient.mutate({
    mutation: gql`
mutation refresh($rtoken: String!) {
  refreshAuth(rtoken: $rtoken)
}`,
    variables: {
      rtoken,
    },
  });
}

function getAdminInfo() {
  return apolloClient.query({
    query: gql`{
myInfo {
  uid, name
}}`,
  });
}


export default {
  login,
  logout,
  refreshAuth,
  getAdminInfo,
};
