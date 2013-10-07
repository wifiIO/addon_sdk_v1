//全局变量，用于维护配置信息
var glb_addon_name = "sweb";
var glb_json_cfg_file_path = "/app/"+glb_addon_name+"/cfg.json";
var glb_statue_invoke_target = "sweb.status";

var sweb_serial_post_target = "/logic/sweb/serial";


var browser_side_config = null;

//加载完成后运行
$(document).ready(
	function(){

		setTimeout(sys_status_info_update, 500);		//延时 通过invoke接口 读取状态json的数据
		setTimeout(sys_cfg_action_update_on_click, 1000);		//延时 读取配置文件

		$("#sys_status_action_update").click(sys_status_info_update);		//按钮 刷新状态
		$("#sys_cfg_action_update").click(sys_cfg_action_update_on_click);		//按钮 刷新配置
		$("#sys_cfg_action_save").click(sys_cfg_action_save_on_click);		//按钮 保存
		$("#sys_cfg_action_reset").click(sys_cfg_action_reset_on_click);		//按钮 复位

		$("#cfg_text_serial_baud input").blur(function(){sys_cfg_check_baud();});
		
		//serial 组织POST请求
		$("#id_sweb_serial_post_target").html(sweb_serial_post_target);
		$("#sweb_param_select_wait select").change(function(){sweb_serial_post_string_update();});
		$("#sweb_param_select_clean select").change(function(){sweb_serial_post_string_update();});
		$("#sweb_param_select_urlenc select").change(function(){sweb_serial_post_string_update();});
		$("#sweb_param_select_mime select").change(function(){sweb_serial_post_string_update();});

		$("#sweb_serial_action_post").click(sweb_serial_action_post_on_click);		//按钮 POST


	});





	////////////////////////////////////////////////////
	//插件管理 探测addon是否加载 进程是否在运行//
	////////////////////////////////////////////////////
	function sys_status_info_update()
	{
		$("#status_run").html("尝试读取...");
		$.ajax({
			url: "/logic/wifiIO/invoke?target=" + glb_statue_invoke_target,
			dataType: "json",
			type: "get",
			success: function (res_js){
				if("undefined" != typeof(res_js.error))
					$("#status_run").html("错误："+ res_js.error);
				else{
					$("#status_run").html("正在运行");
				}

				$("#sweb_status_up_pkt").html(res_js.up_pkt);
				$("#sweb_status_up_byte").html(res_js.up_byte);
				$("#sweb_status_dn_pkt").html(res_js.dn_pkt);
				$("#sweb_status_dn_byte").html(res_js.dn_byte);
			}
		});
	}


	//读取配置文件
	function sys_cfg_action_update_on_click(){
		//read config file
		$.ajax({
			url: glb_json_cfg_file_path,
			dataType: "json",
			type: "get",
			success: sys_cfg_action_update_on_success
		});
	}


	//解析模块的json配置文件
	function sys_cfg_action_update_on_success(module_side_config)
	{
		$("#cfg_text_serial_baud input").val(module_side_config.serial.baud);
		$("#cfg_select_serial_bits select").val(module_side_config.serial.bits);
		$("#cfg_select_serial_sbits select").val(module_side_config.serial.sbits);
		$("#cfg_select_serial_parity select").val(module_side_config.serial.parity);
		$("#cfg_select_serial_rs485 select").val(module_side_config.serial.rs485?"enable":"disable");
		
		browser_side_config = module_side_config;
	}



	//该函数实施保存动作
	function sys_cfg_action_save_on_click()
	{
		var msg = new Array();
		var flag = 0;
		msg.push("错误哦，请检查.");
	
		if(browser_side_config == null){
			msg.push("未能读取源配置文件，或者没有连上模块，请尝试点击刷新按钮.");
		}
	
		//配置内容检查
		flag = sys_cfg_input_check(msg);
	
		if(flag < 0){
			var delay = (msg.length+1)*1000;
			mini_tip(msg.join("<br>"),"error", delay>5000?5000:delay);
			return;
		}
	
		//信息回收
		sys_cfg_info_gathering();
	
		var cfg_json_string = JSON.stringify(browser_side_config);
		console.log(cfg_json_string);
	
		if (confirm("Are you sure to save ?") == false)
			return;
	
		$.ajax({
				url: "/logic/wifiIO/admin?req=postsave&path="+glb_json_cfg_file_path,
				data: cfg_json_string,
				dataType: "json",
				type: "post",
				success: function(){mini_tip("已经保存");}
		});
	}

	//该函数实施复位动作
	function sys_cfg_action_reset_on_click()
	{
		if (confirm("Are you sure to reset ?") == false)
			return;
		$.ajax({
				url: "/logic/wifiIO/admin?req=reset",
				dataType: "json",
				type: "post",
				success: function(){}
		});
	}


	//收集配置信息到配置对象 browser_side_config
	function sys_cfg_info_gathering()
	{
		if(browser_side_config == null){
			alert("似乎没有连上模块，请尝试点击刷新按钮试试.");
			return -1;
		}
	
		browser_side_config.serial.baud = parseInt($("#cfg_text_serial_baud input").val()) ;
		browser_side_config.serial.bits = $("#cfg_select_serial_bits select").val();
		browser_side_config.serial.sbits = $("#cfg_select_serial_sbits select").val();
		browser_side_config.serial.parity = $("#cfg_select_serial_parity select").val();
		browser_side_config.serial.rs485 = $("#cfg_select_serial_rs485 select").val()=="enable"?true:false;
		
	}



	function sys_cfg_check_baud(msg)
	{
		var item_val = $("#cfg_text_serial_baud input").val();
	
		if(integer_check(item_val) < 0){
			ctrl_grp_add_warning("cfg_text_serial_baud");
			if(typeof(msg) != "undefined") msg.push("波特率设置不正确");
			return -1;
		}
		else{
			ctrl_grp_remove_warning("cfg_text_serial_baud","");
			return 0;
		}
	}
	
	//检查组合性的错误	
	function sys_cfg_check_compound(msg)
	{
		return 0;
	}
	
	//配置项检查
	function sys_cfg_input_check(msg)
	{
		var flag = 0;
		flag = ( sys_cfg_check_baud(msg)< 0)? -1:flag;
		flag = ( sys_cfg_check_compound(msg)< 0)? -1:flag;
		return flag;
	}
	function integer_check(port)
	{
		var pattern=/^\d*$/;
		flag = pattern.test(port);
		if(!flag)
			return -1;
		else
			return 0;
	}
	function ip_check(ip)
	{
		var pattern=/^(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9])\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[0-9])$/;
		flag = pattern.test(ip);
		if(!flag)
			return -1;
		else
			return 0;
	}

	
	//添加出错提示信息
	function ctrl_grp_add_warning(ctrl_id, info)
	{
		if(typeof(info) != "undefined")
			$("#"+ctrl_id+" .help-inline").html(info);
		$("#"+ctrl_id).addClass("error");	//添加一个error class
	}
	
	//删除出错提示信息
	function ctrl_grp_remove_warning(ctrl_id, tip)
	{
		if(typeof(tip) != "undefined")
			$("#"+ctrl_id+" .help-inline").html(tip);
		$("#"+ctrl_id).removeClass("error");
	}


//------------------------------ 下面是sweb 控制方面的代码----------------------------------
	
	function sweb_serial_post_string_update()
	{
		var wait = $("#sweb_param_select_wait select").val();
		var clean = $("#sweb_param_select_clean select").val();
		var urlenc = $("#sweb_param_select_urlenc select").val();
		var mime = $("#sweb_param_select_mime select").val();
	
		var idx = 0;
		sweb_serial_post_target = "/logic/sweb/serial";
		if(wait != "null"){
				sweb_serial_post_target = sweb_serial_post_target + (idx > 0?"&wait=":"?wait=") + wait;
				idx++;
		};
		if(clean != "null"){
				sweb_serial_post_target = sweb_serial_post_target + (idx > 0?"&clean=":"?clean=") + clean;
				idx++;
		};
		if(urlenc != "null"){
				sweb_serial_post_target = sweb_serial_post_target + (idx > 0?"&urlenc=":"?urlenc=") + urlenc;
				idx++;
		};
		if(mime != "null"){
				sweb_serial_post_target = sweb_serial_post_target + (idx > 0?"&mime=":"?mime=") + mime;
				idx++;
		};
		$("#id_sweb_serial_post_target").html(sweb_serial_post_target);
	}


	////////////////////////////////////////////////////
	//POST sweb/serial? 请求
	////////////////////////////////////////////////////
	function sweb_serial_action_post_on_click()
	{
		var data_send_to_serial = $("#sweb_serial_text_send textarea").val();

		$("#sweb_serial_text_recv textarea").val("尝试读取...");

		$.ajax({
			url: sweb_serial_post_target,
			dataType: "text",
			data: data_send_to_serial,
			type: "post",
			success: function (res){$("#sweb_serial_text_recv textarea").val(res);}
		});
	}
