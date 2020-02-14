import NProgress from 'nprogress';
import 'nprogress/nprogress.css';

import router from './router';
import store from './store';
import api from './api';

const whiteList = ['/login'];

router.beforeEach((to, from, next) => {
  NProgress.start();
  if (api.getAccessToken()) {
    if (to.path === '/login') {
      next({ path: '/' });
      NProgress.done();
    } else if (!store.getters.myName) {
      store.dispatch('GetInfo').then(() => {
        next();
      }).catch(() => {
        store.dispatch('RefreshJwt').then(() => {
          console.log('JWT Auto refreshed');
          next();
        }).catch(() => {
          store.dispatch('ClearAuth').then(() => {
            console.log('인증에 실패하였습니다. 다시 로그인 해주세요.');
            next({ path: '/login' });
          });
        });
      });
    } else {
      next();
    }
  } else if (whiteList.indexOf(to.path) !== -1) {
    next();
  } else {
    next('/login');
    NProgress.done();
  }
});

router.afterEach(() => {
  NProgress.done();
});
