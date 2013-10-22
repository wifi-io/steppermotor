/**
 * @file			step_motor.c
 * @brief			wifiIO�����������
 *	��������ͺ���28BYJ-48
 *	�Ա�����: http://item.taobao.com/item.htm?id=6895887673
 * @author			dy@wifi.io
*/


#include "include.h"

#define STEPMOTOR_DRIVE_D1 WIFIIO_GPIO_01
#define STEPMOTOR_DRIVE_D2 WIFIIO_GPIO_02
#define STEPMOTOR_DRIVE_D3 WIFIIO_GPIO_03
#define STEPMOTOR_DRIVE_D4 WIFIIO_GPIO_04

/*
ģ��:

�����ʽ: 
{
	"method":"stepmotor.go",
	"params":{
		"speed": 500,
		"clockwise": true,
		"steps":4096
	}
}

speed: �ٶ�
clockwise: �Ƿ�˳ʱ��
steps: �˶�����

*/

#define  NB_NAME "stepmotor"



typedef struct{
	u8_t clockwise;
	u16_t wait;		//��ʱ
	u32_t steps;
}timer_opt_stepmotor_t;

typedef struct {
	u8_t phase;
}stepmotor_opt_t;




// Ԥ���� �ο�:
//http://www.ichanging.org/uln2003-to-drive-relay-and-motor.html
const u8_t phase_bit_map[8] = { 1, 3, 2, 6, 4, 12, 8, 9};	//

static void phase_activate(u8_t phase)
{
	if(phase_bit_map[phase] & 0x01)api_io.high(STEPMOTOR_DRIVE_D1);
	else api_io.low(STEPMOTOR_DRIVE_D1);

	if(phase_bit_map[phase] & 0x02)api_io.high(STEPMOTOR_DRIVE_D2);
	else api_io.low(STEPMOTOR_DRIVE_D2);

	if(phase_bit_map[phase] & 0x04)api_io.high(STEPMOTOR_DRIVE_D3);
	else api_io.low(STEPMOTOR_DRIVE_D3);

	if(phase_bit_map[phase] & 0x08)api_io.high(STEPMOTOR_DRIVE_D4);
	else api_io.low(STEPMOTOR_DRIVE_D4);
}

static void phase_deactivate(void)
{
	api_io.low(STEPMOTOR_DRIVE_D1);
	api_io.low(STEPMOTOR_DRIVE_D2);
	api_io.low(STEPMOTOR_DRIVE_D3);
	api_io.low(STEPMOTOR_DRIVE_D4);
}

static u8_t phase_next(u8_t phase, u8_t clockwise)
{
	phase += sizeof(phase_bit_map);
	if(clockwise) phase++;
	else 	phase--;
	phase = phase % sizeof(phase_bit_map);

	phase_activate(phase);

	return phase;
}




///////////////////////////////////////////
// �ú���Ϊ��ʱ����������Ԥ��ʱ�䱻stepmotor
// ���̵��á�
///////////////////////////////////////////

static int stepmotor_timer(nb_info_t* pnb, nb_timer_t* ptmr, void *ctx)
{
	stepmotor_opt_t *motor_opt = (stepmotor_opt_t*) api_nb.info_opt(pnb);
	timer_opt_stepmotor_t* tmr_opt = (timer_opt_stepmotor_t*)api_nb.timer_opt(ptmr);


	motor_opt->phase = phase_next(motor_opt->phase, tmr_opt->clockwise);

	tmr_opt->steps --;

	if(tmr_opt->steps == 0 ){
		phase_deactivate();
		return NB_TIMER_RELEASE;
	}
	else
		return tmr_opt->wait;


}

////////////////////////////////////////
//�ú�����stepmotor�������У�ʵ�� timer����
////////////////////////////////////////
static int invoke_set_step_timer(nb_info_t*pnb, nb_invoke_msg_t* msg)
{
	//��ȡ������ߵ���ͼ
	timer_opt_stepmotor_t* opt = api_nb.invoke_msg_opt(msg);


	//���氲�ź��ʵ�timer�������ĳ���ƾߵĿ���
	nb_timer_t* ptmr;

	//�����Ѿ����ţ���timer�ڵȴ��У���λ��timer
	ptmr = api_nb.timer_by_ctx(pnb, NULL);	

	if(NULL == ptmr){	//��û�о������µ�
		LOG_INFO("stepmotor:tmr alloced.\r\n");
		ptmr = api_nb.timer_attach(pnb, opt->wait, stepmotor_timer, NULL, sizeof(timer_opt_stepmotor_t));	//����alloc
	}
	else{	//������ ���¶��������
		LOG_INFO("stepmotor:tmr restarted.\r\n");
		api_nb.timer_restart(pnb, opt->wait, stepmotor_timer);
	}

	//timer�����Ķ�������
	timer_opt_stepmotor_t* tmr_opt = (timer_opt_stepmotor_t*)api_nb.timer_opt(ptmr);
	string.memcpy(tmr_opt, opt, sizeof(timer_opt_stepmotor_t));	//����Ҫ�Ĳ��� ��ֵ��timer

	return STATE_OK;
}







////////////////////////////////////////
//�ú��������Ĳ��� stepmotor����
//�޷���֤�޾��������ʹ��nb����ṩ��
//invoke���ƣ���stepmotor����"ע��"���к���
////////////////////////////////////////

static int stepmotor_movement_arrange(nb_info_t* pnb, u32_t steps, u8_t clockwise, u16_t speed)
{

	LOG_INFO("DELEGATE: steps:%u speed:%u %s.\r\n", steps, speed, clockwise?"clockwise":"anti_clockwise");

	//����һ��invoke������Ҫ������(��Ϣ)
	nb_invoke_msg_t* msg = api_nb.invoke_msg_alloc(invoke_set_step_timer, sizeof(timer_opt_stepmotor_t), NB_MSG_DELETE_ONCE_USED);
	timer_opt_stepmotor_t* opt = api_nb.invoke_msg_opt(msg);

	//��д��Ӧ��Ϣ
	opt->clockwise = clockwise;
	opt->steps = steps;
	opt->wait = 1000/speed;
	if(opt->wait < 1)
		opt->wait = 1;

	if(opt->wait > 1000)
		opt->wait = 1000;

	//���͸�stepmotor���� ʹ������ invoke_set_step_timer �������
	api_nb.invoke_start(pnb , msg);
	return STATE_OK;
}






////////////////////////////////////////
// �ú��������������� �ǵ��ø�ί�нӿڵĽ���
// httpd��otd �����Ե��øýӿ�
// �ӿ�����Ϊ stepmotor.go������:
//{
//	"method":"stepmotor.go",
//	"params":{
//		"speed": 500,
//		"clockwise": true,
//		"steps":4096
//	}
//}
////////////////////////////////////////

int JSON_RPC(go)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	//�̳���fp_json_delegate_start
{
	int ret = STATE_OK;
	char* err_msg = NULL;
	nb_info_t* pnb;
	if(NULL == (pnb = api_nb.find(NB_NAME))){		//�жϷ����Ƿ��Ѿ�����
		ret = STATE_ERROR;
		err_msg = "Service unavailable.";
		LOG_WARN("stepmotor:%s.\r\n", err_msg);
		goto exit_err;
	}

	LOG_INFO("DELEGATE stepmotor.go.\r\n");


/*
	{
		"speed": xxx,
		"clockwise": true,
		"steps":1234
	}
*/

	u8_t clockwise = 0;
	u16_t speed = 1;
	u32_t steps = 0;
	
	if(STATE_OK != jsmn.key2val_uint(pjn->js, pjn->tkn, "steps", &steps) ||
		STATE_OK != jsmn.key2val_bool(pjn->js, pjn->tkn, "clockwise", &clockwise) ||
		STATE_OK != jsmn.key2val_u16(pjn->js, pjn->tkn, "speed", &speed)	){

		ret = STATE_ERROR;
		err_msg = "params error .";
		LOG_WARN("stepmotor:%s.\r\n", err_msg);
		goto exit_err;
	}

	
	stepmotor_movement_arrange(pnb, steps, clockwise, speed);


//exit_safe:
	if(ack)
		jsmn.delegate_ack_result(ack, ctx, ret);
	return ret;

exit_err:
	if(ack)
		jsmn.delegate_ack_err(ack, ctx, ret, err_msg);
	return ret;
}


////////////////////
//����nb��ܵ��ûص�
////////////////////


	///////////////////////////
	//����nb��Ϣѭ��֮ǰ������
	///////////////////////////
static  int enter(nb_info_t* pnb)
 {
//	stepmotor_opt_t *opt = (stepmotor_opt_t*) api_nb.info_opt(pnb);

	 api_io.init(STEPMOTOR_DRIVE_D1, OUT_PUSH_PULL);
	 api_io.init(STEPMOTOR_DRIVE_D2, OUT_PUSH_PULL);
	 api_io.init(STEPMOTOR_DRIVE_D3, OUT_PUSH_PULL);
	 api_io.init(STEPMOTOR_DRIVE_D4, OUT_PUSH_PULL);

	phase_deactivate();
	//phase_activate(phase_bit_map[opt->phase]);

	LOG_INFO("stepmotor:phase initialized.\r\n");

	//////////////////////////////////////////////////////
	//����STATE_OK֮���ֵ������������Ϣѭ����ֱ���˳�����
	//////////////////////////////////////////////////////
 	return STATE_OK;
 }
 
	///////////////////////////
	//nb���յ��˳���Ϣʱ������
	///////////////////////////
static  int before_exit(nb_info_t* pnb)
 {
 	return STATE_OK;
 }
 
	///////////////////////////
	//�˳�nb��Ϣѭ��֮�󱻵���
	///////////////////////////
static  int exit(nb_info_t* pnb)
 {
 	return STATE_OK;
 }

////////////////////
//�ص�������ϳɽṹ
////////////////////

static const nb_if_t nb_stepmotor_if = {enter, before_exit, exit};





////////////////////////////////////////
//ÿһ��addon����main���ú����ڼ��غ�����
// �����ʺ�:
// addon�������л�����飻
// ��ʼ��������ݣ�

//������ �� ADDON_LOADER_GRANTED �����º������غ�
//��ж��

//�ú��������������� ��wifiIO����
////////////////////////////////////////

int main(int argc, char* argv[])
{
	nb_info_t* pnb;

	if(NULL != api_nb.find(NB_NAME)){		//�жϷ����Ƿ��Ѿ�����
		LOG_WARN("main:Service exist,booting abort.\r\n");
		goto err_exit;
	}


	////////////////////////////////////////
	//�������ǽ���һ��nb��ܳ���Ľ��̣�
	//����ΪNB_NAME 
	////////////////////////////////////////


	if(NULL == (pnb = api_nb.info_alloc(sizeof(stepmotor_opt_t))))	//����nb�ṹ
		goto err_exit;

	stepmotor_opt_t *opt = (stepmotor_opt_t*) api_nb.info_opt(pnb);
	opt->phase = 0;

	api_nb.info_preset(pnb, NB_NAME, &nb_stepmotor_if);	//���ṹ

	if(STATE_OK != api_nb.start(pnb)){		//��������
		LOG_WARN("main:Start error.\r\n");
		api_nb.info_free(pnb);
		goto err_exit;
	}

	LOG_INFO("main:Service starting...\r\n");


	return ADDON_LOADER_ABORT;	//��������˵��nb�����ȫ�����ˣ�����Ҫפ��
//	return ADDON_LOADER_GRANTED;
err_exit:
	return ADDON_LOADER_ABORT;
}




//httpd�ӿ�  ��� json
/*
GET http://192.168.1.105/logic/wifiIO/invoke?target=stepmotor.status

{"state":loaded}

*/


int __ADDON_EXPORT__ JSON_FACTORY(status)(char*arg, int len, fp_consumer_generic consumer, void *ctx)
{
	nb_info_t* pnb;
	char buf[32];
	int n, size = sizeof(buf);

	if(NULL == (pnb = api_nb.find(NB_NAME))){		//�жϷ����Ƿ��Ѿ�����
		goto exit_noload;
	}

	stepmotor_opt_t *opt = (stepmotor_opt_t*) api_nb.info_opt(pnb);


	n = 0;
	n += utl.snprintf(buf + n, size-n, "{\"state\":\"loaded\", \"phase\":%u}", opt->phase);
	return  consumer(ctx, (u8_t*)buf, n);

exit_noload:
	n = 0;
	n += utl.snprintf(buf + n, size-n, "{\"state\":\"not loaded\"}");

	return  consumer(ctx, (u8_t*)buf, n);
}






