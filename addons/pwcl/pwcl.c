
/**
 * @file			pwcl.c
 * @brief			Pulse Width Code Language 脉宽编码描述语言
 *
 *					详情查看read.md
 * @author			dy@wifi.io
*/


#include "include.h"



/*
PWC: Pulse Width Codec


	width:: [+|-]integer

	width_seq:: /(width(,width)* /)

	width_until:: /<width/>

	seg_ref:: /[integer/]

	op_exp:: integer'*'width_seq	|  integer'*'seg_ref	| integer'*'width_until

	pwc_segment:: (width_seq|width_until|seg_ref|op_exp)+	

	pwc:: (pwc_segment;)+

	characters: '('  ')' '['  ']' '<' '>' '*'  ','  '+'  '-'  Integer

*/

#define PWC_PIN WIFIIO_GPIO_01

#define PMC_CODE_MAX 256

#define STAT_PWC_PAUSE	1
#define STAT_PWC_HALT	2

#define PMC_SEG_STACK_DEEPTH 16

#define PWC_STATE_IDLE 0
#define PWC_STATE_RUNNING 1

typedef struct {
	u8_t Head;
	u8_t End;
	u8_t Cyc;
	u8_t Ptr;	
}segment_ctx_t;


typedef struct {
	char pwc_raw[PMC_CODE_MAX];
	segment_ctx_t* segctx_top;
	segment_ctx_t* segctx_base;
	u32_t time;		//全局时间
	u16_t onload;	//最近一次更新的脉宽
	s8_t sign;		//当前极性
	s8_t plen;		//pwc_raw 中pattern部分的长度
	char start_flag;
}pwc_t;


#define TOP_CTX (pwc->segctx_top)
#define IS_EMPTY (pwc->segctx_top == pwc->segctx_base)


pwc_t glb_pwc;
segment_ctx_t stack[PMC_SEG_STACK_DEEPTH];

///////////////////////////////////////////////

char* modified_atou(char* c, unsigned int *out)
{
	unsigned int dig = 0;
	while(_isdigit(*c)){
		dig = dig*10 + *c - '0';
		c++;
	}
	*out = dig;
	return c;
}



///////////////////////////////////////////////




static int pwout(pwc_t* pwc, s8_t sign, u16_t width)
{
	s8_t o_sign = (sign == 0)? pwc->sign: sign;
	
	//rt_kprintf(o_sign > 0? ",%d":",-%d",width);
	pwc->onload = width;
	pwc->sign = o_sign;

	pwc->time += width;

	if(width)
		return STAT_PWC_PAUSE;
	else
		return 0;

}

static int pwout_until(pwc_t* pwc, s8_t sign, u16_t width)
{
	s8_t o_sign;

	//<0>表示重置全局时间
	if(width == 0){
		pwc->time = 0;
		goto safe_exit;
	}

	//小于则减少全局时间
	if(width <= pwc->time){
		pwc->time -= width;
		goto safe_exit;
	}

	//大于则产生等候width
	else{
		width -= pwc->time;
		pwc->time = 0;
	}

	o_sign = (sign == 0)? pwc->sign: sign;

	pwc->onload = width;
	pwc->sign = o_sign;



safe_exit:
	//如果有width输出，才中断，否则继续
	if(pwc->onload)
		return STAT_PWC_PAUSE;
	else
		return 0;
}




static int pwc_segctx_pop(pwc_t* pwc)
{

	segment_ctx_t* tops = pwc->segctx_top;

	//如果有循环，继续当前Ctx
	if(tops->Cyc > 0){
		tops->Cyc--;
		tops->Ptr = tops->Head;
		return 0;
	}

	//如果堆栈还有内容，弹出
	if(tops > pwc->segctx_base){
		tops --;
		pwc->segctx_top = tops;
	}
	
	//空
	else
		return STAT_PWC_HALT;

	
	return 0;

}

static int pwc_segctx_push(	pwc_t* pwc,u8_t Head,u8_t End,u8_t Cyc)
{
	segment_ctx_t* tops = pwc->segctx_top;
	if(tops - pwc->segctx_base + sizeof(segment_ctx_t) < PMC_SEG_STACK_DEEPTH * sizeof(segment_ctx_t)){
		tops++;
		tops->Head = tops->Ptr = Head;
		tops->End = End;
		tops->Cyc = (Cyc>0?Cyc-1:Cyc);
		pwc->segctx_top = tops;
		return STATE_OK;
		
	}else
		return STATE_FULL;
}


static int pwc_segment(pwc_t* pwc,int s, u8_t *h,u8_t *e)
{
	u8_t i = 0;
	char* c = pwc->pwc_raw;

	if(s == 0)
		*h = 0;

	while(c[i] != '\0'){
		
		if(s == 0 && c[i] == ';'){
			break;
		}

		if((s > 0) && (c[i] == ';') && (--s == 0)){
			*h = i+1;
		}

		i++;
	}

	*e = i;
	
	if(s > 0)
		return -1;
	else
		return 0;

}




//ret 0:OK
//w, s represent width & sign
//分解 "[+|-]123"    格式字符串
static char* width_parser(int *o_ret,char *i_p, s8_t *o_s, u16_t *o_w)
{
	char* p = i_p;
	u32_t v = 0;
	if(_isdigit(*p)){
		*o_s = 0;
	}
	else if(*p == '+'){
		*o_s = 1;
		p++;
	}
	else if(*p == '-'){
		*o_s = -1;
		p++;
	}
	else{
		*o_ret = -1;
		return p;
	}

	p = modified_atou(p, (unsigned int*)&v);
	*o_w = v;
	return p;

}



int pwc_set_code(pwc_t* pwc, char* c)
{
	char* p = pwc->pwc_raw;
	u8_t i = 0, h = 0;

	_strcpy(pwc->pwc_raw, c);
	pwc->plen = _strlen(c);

	//复位状态机
	pwc->time = 0;
	pwc->sign = -1;
	pwc->segctx_top = pwc->segctx_base = stack;
	pwc->onload = 0;

	//堆栈清理利于分析使用情况；
 	_memset(stack, 0xA5, sizeof(stack));

	//找到最后一个segment 压入堆栈；
	while(p[i] != '\0'){
		if(p[i] == ';' && p[i+1] != '\0'){
			h = i+1;
		}
		i++;
	}
	pwc_segctx_push(pwc,h,i,1);
	return 0;
}




int pwc_caculate_next(pwc_t* pwc,char *err)
{
	
	int ret = 0;
	//char* perr = (void*)0;
	char *p, *q;

safe_ctx_begin:

	//检查待解码长度或者堆栈，判断是否能继续
	while(TOP_CTX->Ptr == TOP_CTX->End){
		ret = pwc_segctx_pop(pwc);
		if( ret != 0){
			//perr = os_str_push(err, "...Done.");
			goto safe_exit;
		}
	};

	p = pwc->pwc_raw + TOP_CTX->Ptr;
	q = p;


same_ctx_repeat:	//不管哪里流向这里都要保证p == q


		switch(*q){

			case('('):	//width_seq	 like" -> (xx,xx)"
			case(','):	//width_seq	 like"(xx           ->,xx)"
			{
				u16_t w = 0;
				s8_t s = 0;
				q += 1;

				//分解其后的整数
				q = width_parser(&ret, q, &s, &w);//q: "(xx           ->,xx)" or "(xx,xx           ->)"
				TOP_CTX->Ptr += q - p;
				p = q;

				if(0 != ret){
					//perr = os_str_push(err, "Width parser err.");
					goto err_safe_exit;
				}


				//输出
				ret = pwout(pwc, s,w);


				//若之后是")"，指针直接移开
				if(')' == *q ){
					TOP_CTX->Ptr += 1;	// "(xx ,xx)          ->xx"


					if(STAT_PWC_PAUSE  == ret)
						goto safe_exit;



					//并且ctx解析完毕；
					if(TOP_CTX->Ptr == TOP_CTX->End)
						goto safe_ctx_begin;

					q++; p = q;
				}


				if(STAT_PWC_PAUSE  == ret)
					goto safe_exit;

				//要么是","  , q: "(xx           ->,xx)xx",或者还没有解析完
				//要么是")          ->xx"
				goto same_ctx_repeat;

			}
			//	break;



			case('['):
			{
				u32_t idx = 0;
				u8_t h = 0,e = 0;

				q += 1;	//q: "[     ->xx]xx"

				//p = q;
				q = modified_atou(q, (unsigned int *)&idx) + 1;	//q: "[xx]     ->xx"
				if(	(q-p == 2)||	//in case "[]" or "[asc]"
					(*(q-1) != ']')|| //in case "[00asc]"
					0 != (ret = pwc_segment(pwc,idx, &h,&e))){
					//perr = os_str_push(err, "[] err.");
					goto err_safe_exit;
				}

				//如果ctx的首尾正好是[]，那么直接替换CTX的首尾
				if(TOP_CTX->Ptr == TOP_CTX->Head && TOP_CTX->Ptr + (q - p) == TOP_CTX->End){
					TOP_CTX->Ptr = TOP_CTX->Head = h;
					TOP_CTX->End = e;

					p = pwc->pwc_raw + TOP_CTX->Ptr;
					q = p;

					goto same_ctx_repeat;
				}

				//将当前ctx指向下一单元
				TOP_CTX->Ptr += q - p;
				p = q;

				//将[]替换内容压入堆栈
				ret = pwc_segctx_push(pwc,h, e, 1);
				if(ret){
					//perr = os_str_push(err, "stack full.");
					goto err_safe_exit;
				}
				goto safe_ctx_begin;

			}

			//	break;

			case('<'):
			{
				u16_t w = 0;
				s8_t s = 0;
				q += 1;

				//分解其后的整数
				q = width_parser(&ret, q, &s, &w)+1;//q: "<xx>           ->xx" 
				TOP_CTX->Ptr += q - p;
				p = q;
				if(0 != ret){
					//perr = os_str_push(err, "<xx> Width parser err.");
					goto err_safe_exit;
				}


				//输出
				ret = pwout_until(pwc,s,w);
				if(STAT_PWC_PAUSE  == ret)
					goto safe_exit;


				if(TOP_CTX->Ptr < TOP_CTX->End)
					goto same_ctx_repeat;
				else
					goto safe_ctx_begin;

			}

			//	break;

			default:

				//若为数值，应为循环
				if(_isdigit(*q)){	//(xxx)     ->2*[xx]

					u32_t cnt = 0;
					q = modified_atou(q, (unsigned int*)&cnt) + 1;	//q: "(xxx)2*     ->[xx]"
					TOP_CTX->Ptr += q - p;
					p = q;

#ifdef NONE_CYCLE_WIDTH_UNTIL
					//排除错误：循环为0；之后不是*；循环节不是()or[]
					if(cnt == 0 || *(q-1) != '*' || ((*q != '[')&&(*q != '(')))
#else
					//排除错误：循环为0；之后不是*；循环节不是()or[]or<>
					if(cnt == 0 || *(q-1) != '*' || ((*q != '[')&&(*q != '(')&&(*q != '<')))
#endif
					{	//perr = os_str_push(err, "Repeat err.");
						ret = -1;
						goto err_safe_exit;
					}


					//若循环次数大于1，那么压栈，安全循环
					if(cnt > 1){
						u8_t h = TOP_CTX->Ptr;

						//1 这里比较危险 ，没有长度检查

#ifdef NONE_CYCLE_WIDTH_UNTIL
						while((*q != ']') && (*q != ')'))q++; 
#else
						while((*q != ']') && (*q != ')') && (*q != '>'))q++; 
#endif
						q+=1;

						TOP_CTX->Ptr += q - p;
						p = q;

						ret = pwc_segctx_push(pwc, h, TOP_CTX->Ptr,cnt);
						if(ret){
							//perr = os_str_push(err, "stack full.");
							goto err_safe_exit;
						}
						goto safe_ctx_begin;
					}

					//若循环次数等于1，直接处理之后的"("
					goto same_ctx_repeat;


				}
				else{
					ret = -1;
					//perr = os_str_push(err, "err char");
					goto err_safe_exit;
				}

				//break;


		}





safe_exit:
err_safe_exit:


	return ret;
}



void io_pwc_start(pwc_t* pwc)
{
	pwc->start_flag = PWC_STATE_RUNNING;
	LOG_INFO("Next pulse %u us.\r\n", pwc->onload);

	api_tim.tim6_start(1000);	//1000us后开始
}


void io_pwc_stop(pwc_t* pwc)
{
	pwc->start_flag = PWC_STATE_IDLE;
	api_io.toggle(WIFIIO_GPIO_02);
}


void io_pwc_iteration(pwc_t* pwc) 
{

	while(!api_tim.tim6_is_timeout())
		if(PWC_STATE_IDLE == pwc->start_flag)return;	//等到时间点 再动作

	//最要紧尽快处理的事情
	if(pwc->sign > 0)
		api_io.high(PWC_PIN);	//IO_GPIO->BSRR = IO7_PIN;
	else if(pwc->sign < 0)
		api_io.low(PWC_PIN);	//IO_GPIO->BRR = IO7_PIN;

	if(pwc->onload > 0){
		api_tim.tim6_start(pwc->onload);		//更改定时器时间
 
		//准备新的延时
		pwc->onload = 0;
		pwc_caculate_next(pwc,(void*)0);

		//可能在产生最后一个width时HALT
		//if(STAT_PWC_HALT == pwc_caculate_next(pwc,(void*)0))			io_pwc_stop();
	}
	else	//关闭
		io_pwc_stop(pwc);

}



int main(int argc, char* argv[])
{
	api_tim.tim6_init(1000000);	//初始化定时器6 1us为单位


	api_io.init(PWC_PIN, OUT_PUSH_PULL);
	api_io.high(PWC_PIN);

	api_io.init(WIFIIO_GPIO_02, OUT_PUSH_PULL);
	api_io.high(WIFIIO_GPIO_02);



	//安装代码
	pwc_set_code(&glb_pwc, "(-10000);(+10000);100*[0];100*[1];[3][2];20*[4];");

	if(STAT_PWC_PAUSE == pwc_caculate_next(&glb_pwc,(void*)0)){	//计算第一个计数值
		//那么执行之
		io_pwc_start(&glb_pwc);
	}
	else{
		LOG_WARN("pwc:Caculate error.\r\n");
		return ADDON_LOADER_ABORT;
	}

	while(1){
		if(PWC_STATE_RUNNING == glb_pwc.start_flag)
			io_pwc_iteration(&glb_pwc);
		else
			_tick_sleep(100);
	}


	//进程可以退出 但是代码不能释放
	return ADDON_LOADER_GRANTED;	//ADDON_LOADER_ABORT;
}






/*
////////////////////////////////////////
Json delegate interface are mostly used for callbacks, and is supported by both Httpd & OTd.

So if you want your code be called from web browser or from cloud(OTd),you write code in the form below.

int JSON_RPC(xxx)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx){}

httpd will receive POST request like:
{
	"method":"pwc.code",
	"params":"(-10000);(+10000);100*[0];100*[1];[3][2];50*[4];"
}
////////////////////////////////////////
*/

int JSON_RPC(code)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	//继承于fp_json_delegate_start
{
	int ret = STATE_OK;
	char* err_msg = NULL;
	char pwmcode[256];

	LOG_INFO("DELEGATE pwc.code \r\n");



/*
{
	"method":"pwc.code",
	"params":"(-10000);(+10000);100*[0];100*[1];[3][2];50*[4];"
}
*/

//	pjn->js point to very begining of json string.
//	pjn->tkn is the token object which points to json element "0123456789ABCDEF"
//	Google "jsmn" (super lightweight json parser) for detailed info.

	if(NULL == pjn->tkn || 
	    JSMN_STRING != pjn->tkn->type ||											//must be string
	    STATE_OK != jsmn.tkn2val_str(pjn->js, pjn->tkn, pwmcode, sizeof(pwmcode), NULL)){		//parse string into binary array
		ret = STATE_ERROR;
		err_msg = "param error.";
		LOG_WARN("pwc:%s.\r\n", err_msg);
		goto exit_err;
	}

	if(PWC_STATE_IDLE == glb_pwc.start_flag){	//idle
		LOG_INFO("New code:%s\r\n", pwmcode);
		ret = pwc_set_code(&glb_pwc, pwmcode);
		if(STATE_OK == ret && STAT_PWC_PAUSE == pwc_caculate_next(&glb_pwc,(void*)0)){
			//那么执行之
			io_pwc_start(&glb_pwc);
		}
		else
			LOG_WARN("Start error.\r\n");
	}
	else{
		ret = STATE_ERROR;
		err_msg = "busy.";
		LOG_WARN("pwc:%s.\r\n", err_msg);
		goto exit_err;
	}



//exit_safe:
	if(ack)
		jsmn.delegate_ack_result(ack, ctx, ret);	//just reurn {"result":0}
	return ret;

exit_err:
	if(ack)
		jsmn.delegate_ack_err(ack, ctx, ret, err_msg);	//return {"error":{"code":xxx, "message":"yyyy"}}
	return ret;
}




