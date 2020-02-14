import Vue from 'vue';
import Vuex from 'vuex';

import api from './api';

Vue.use(Vuex);

const KEY_RTOKEN = 'rtoken';

function localStorageGetNumber(key, dflt) {
  const val = localStorage.getItem(key);
  return val === null ? dflt : Number(val);
}

function localStorageGetString(key) {
  const val = localStorage.getItem(key);
  if (!val || val === 'undefined') return '';
  return val;
}

function localStorageSet(key, val) {
  if (val) {
    localStorage.setItem(key, val);
  } else {
    localStorage.removeItem(key);
  }
}

export default new Vuex.Store({
  state: {
    refreshToken: localStorageGetString(KEY_RTOKEN),
    myUid: '',
    myName: '',
  },
  getters: {
    refreshToken: state => state.refreshToken,
    myUid: state => state.myUid,
    myName: state => state.myName,
  },
  mutations: {
    CLEAR_AUTH: (state) => {
      console.log('called clear_AUTH');
      api.setAccessToken(undefined);
      localStorage.removeItem(KEY_RTOKEN);
      state.refreshToken = undefined;
      state.myUid = '';
      state.myName = '';
    },
    SET_ACC_TOKEN: (state, token) => {
      api.setAccessToken(token);
    },
    SET_RFS_TOKEN: (state, token) => {
      state.refreshToken = token;
      localStorage.setItem(KEY_RTOKEN, token);
    },
    SET_ADMIN_INFO: (state, info) => {
      state.myUid = info.uid;
      state.myName = info.name;
    },
  },
  actions: {
    Login({ commit }, userInfo) {
      return new Promise((resolve, reject) => {
        api.login(userInfo.userid.trim(), userInfo.passwd).then(({ data }) => {
          if (data && data.login) {
            const { accessToken, refreshToken } = data.login;
            commit('SET_ACC_TOKEN', accessToken);
            commit('SET_RFS_TOKEN', refreshToken);
            resolve();
          } else {
            reject(new Error('로그인 ID 또는 비밀번호가 틀립니다.'));
          }
        }).catch((error) => {
          reject(error);
        });
      });
    },
    RefreshJwt({ commit, state }) {
      return new Promise((resolve, reject) => {
        if (state.refreshToken) {
          api.refreshAuth(state.refreshToken).then(({ data }) => {
            commit('SET_ACC_TOKEN', data.refreshAuth);
            resolve();
          }).catch(reject);
        } else {
          reject();
        }
      });
    },
    GetInfo({ commit }) {
      return new Promise((resolve, reject) => {
        api.getAdminInfo().then(({ data }) => {
          commit('SET_ADMIN_INFO', data.myInfo);
          resolve(data);
        }).catch(reject);
      });
    },
    Logout({ commit, state }) {
      console.log('Logout called');
      return new Promise((resolve, reject) => {
        api.logout(state.refreshToken).then(() => {
          resolve();
        }).catch((error) => {
          console.error('Logout failed:', error);
          reject(error);
        });
        commit('CLEAR_AUTH');
      });
    },
    ClearAuth({ commit }) {
      return new Promise((resolve) => {
        commit('CLEAR_AUTH');
        resolve();
      });
    },
  },
});
