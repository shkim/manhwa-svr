import gql from 'graphql-tag';
import { apolloClient, apiHttp } from './base';

const uploadHeader = {
  'Content-Type': 'multipart/form-data',
};

function uploadSampleFile(sampleId, rename, file) {
  const frm = new FormData();
  frm.set('sample', sampleId);
  frm.set('rename', rename);
  frm.append('file', file);
  return apiHttp.post('/file', frm, { headers: uploadHeader });
}

function getTitleInfo(tid) {
  return apolloClient.query({
    query: gql`
      query titleInfo($tid: ID!) {
        titleInfo(tid: $tid) {
          name, email, data
        }
      }`,
    variables: {
      tid,
    },
  });
}

function createTitle(name) {
  return apolloClient.mutate({
    mutation: gql`
      mutation titleCreate($name: String!) {
        titleCreate(name: $name)
      }`,
    variables: {
      name,
    },
  });
}

export default {
  uploadSampleFile,
  getTitleInfo,
  createTitle,
};
