import Vue from 'vue';
import VueApollo from 'vue-apollo';

import Antd from 'ant-design-vue';
import App from './App.vue';
import router from './router';
import store from './store';
import { apolloClient } from './api/base';

import 'ant-design-vue/dist/antd.css';
import './assets/app.styl';
import './permission';

Vue.config.productionTip = false;

Vue.use(Antd);

// let loading = 0;
const apolloProvider = new VueApollo({
  clients: {
    a: apolloClient,
  },
  defaultClient: apolloClient,
  defaultOptions: {
    $loadingKey: 'loading',
  },
  /* watchLoading(state, mod) {
    loading += mod;
    // console.log('Global loading', loading, mod);
  }, */
  errorHandler(error) {
    console.log('Global error handler');
    console.error(error);
  },
});

function refreshAccToken() {
  if (store.getters.refreshToken) {
    store.dispatch('RefreshJwt').catch(() => {
      router.push({ path: '/login' });
    });
  }
}

new Vue({
  router,
  store,
  apolloProvider,
  mounted() {
    this.interval = setInterval(refreshAccToken, 30 * 60000); // 30 min
  },
  beforeDestroy() {
    clearInterval(this.interval);
  },
  render: (h) => h(App),
}).$mount('#app');
