import Vue from 'vue';
import VueRouter from 'vue-router';
import Layout from './views/Layout.vue';
import Home from './views/Home.vue';

Vue.use(VueRouter);

const router = new VueRouter({
  mode: 'history',
  base: process.env.BASE_URL,
  scrollBehavior: () => ({ y: 0 }),
  routes: [
    {
      path: '/',
      redirect: '/app/main',
    },
    {
      path: '/login',
      name: 'Login',
      // route level code-splitting
      // this generates a separate chunk (about.[hash].js) for this route
      // which is lazy-loaded when the route is visited.
      component: () => import(/* webpackChunkName: "about" */ './views/Login.vue'),
    },
    {
      path: '/app',
      component: Layout,
      redirect: '/app/main',
      children: [
        {
          path: 'main',
          name: 'Home',
          component: Home,
        },
        /* {
          path: 'title/:titleId',
          name: 'Title',
          component: () => import('./views/Title.vue'),
        }, */
      ],
    },
    /* { path: '/404', component: () => import('./views/404.vue') },
    { path: '/50x', component: () => import('./views/50x.vue') },
    { path: '*', redirect: '/404' }, */
  ],
});

export default router;
