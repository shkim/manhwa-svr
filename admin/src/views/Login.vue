<template>
  <div class="loginPage">
    <main>

      <h1>만화 관리자</h1>

      <a-form
        :form="form"
        @submit="onSubmit"
      >
        <a-alert
          type="error"
          :closable="true"
          v-show="errMsg"
          :message="errMsg"
          showIcon
        />

        <a-form-item
          label="로그인 ID"
        >
          <a-input
            v-decorator="['userid', { rules: [{ required: true, message: '로그인 ID를 입력해 주십시오.' }] }]"
            allowClear
            size="large"
            placeholder="로그인 ID"
          >
            <a-icon
              slot="prefix"
              type="user"
            />
          </a-input>
        </a-form-item>

        <a-form-item
          label="비밀번호"
        >
          <a-input-password
            v-decorator="[
              'passwd',
              { rules: [{ required: true, message: '비밀번호를 입력해 주십시오.' }] },
            ]"
            allowClear
            size="large"
            placeholder="비밀번호"
          >
            <a-icon
              slot="prefix"
              type="lock"
            />
          </a-input-password>
        </a-form-item>

        <a-form-item>
          <a-button
            :loading="waiting"
            size="large"
            block
            clearable
            html-type="submit"
            type="primary"
          >로그인</a-button>
        </a-form-item>
      </a-form>
    </main>
  </div>
</template>


<style lang="stylus">
.loginPage
  display flex
  justify-content center
  align-items center
  position absolute
  height 100%
  width 100%
  background-color #eee

  main
    width 400px
    padding-bottom 15%

  h1
    text-align center
    font-size 3rem
    color #333

  .ant-alert
    margin-bottom 2rem

</style>

<script>
import api from '../api';

export default {
  name: 'Login',
  data() {
    return {
      waiting: false,
      errMsg: '',
      form: this.$form.createForm(this, { name: 'coordinated' }),
    };
  },
  computed: {
  },
  methods: {
    onSubmit(e) {
      e.preventDefault();
      this.form.validateFields((err, values) => {
        if (!err) {
          this.$store.dispatch('Login', values).then(() => {
            this.waiting = true;
            this.$router.push({ path: '/app/main' });
          }).catch(err2 => {
            this.waiting = false;
            const code = api.getGqlErrorCode(err2);
            switch (code) {
              case 'LOGIN_FAIL':
                this.errMsg = '로그인 정보가 올바르지 않습니다.';
                break;
              case 'E500':
                this.errMsg = '서버에서 오류가 발생하였습니다.';
                break;
              default:
                if (err2.message && err2.message.indexOf('Network error:') >= 0) {
                  this.errMsg = '서버에 연결할 수 없습니다.';
                } else {
                  this.errMsg = err2.message;
                }
            }
          });
        }
      });
    },
  },
};
</script>
