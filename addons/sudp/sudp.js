//全局变量，用于维护配置信息
glb_addon_name = "sudp"
glb_json_cfg_file_path = "/app/"+glb_addon_name+"/cfg.json"
glb_statue_invoke_target = "sudp.status"

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


		$("#cfg_text_net_dip input").blur(function(){sys_cfg_check_dip();});
		$("#cfg_text_net_dport input").blur(function(){sys_cfg_check_dport();});
		$("#cfg_text_net_lport input").blur(function(){sys_cfg_check_lport();});
		$("#cfg_text_serial_baud input").blur(function(){sys_cfg_check_baud();});
		$("#cfg_text_other_holdtime input").blur(function(){sys_cfg_check_holdtime();});
		$("#cfg_text_other_loadmax input").blur(function(){sys_cfg_check_loadmax();});


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

				$("#sudp_status_up_pkt").html(res_js.up_pkt);
				$("#sudp_status_up_byte").html(res_js.up_byte);
				$("#sudp_status_dn_pkt").html(res_js.dn_pkt);
				$("#sudp_status_dn_byte").html(res_js.dn_byte);
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
		$("#cfg_text_net_dip input").val(module_side_config.net.dip);
		$("#cfg_text_net_dport input").val(module_side_config.net.dport);
		$("#cfg_text_net_lport input").val(module_side_config.net.lport);

		$("#cfg_text_serial_baud input").val(module_side_config.serial.baud);
		$("#cfg_select_serial_bits select").val(module_side_config.serial.bits);
		$("#cfg_select_serial_sbits select").val(module_side_config.serial.sbits);
		$("#cfg_select_serial_parity select").val(module_side_config.serial.parity);
		$("#cfg_select_serial_rs485 select").val(module_side_config.serial.rs485?"enable":"disable");
		
		$("#cfg_text_other_holdtime input").val(module_side_config.holdtime);
		$("#cfg_text_other_loadmax input").val(module_side_config.loadmax);

		browser_side_config = module_side_config;
	}



	//该函数实施保存动作
	function sys_cfg_action_save_on_click()
	{
		var msg = new Array();
		var flag = 0;
		msg.push("错误哦，请检查.");
	
		if(browser_side_config == null){
			msg.push("似乎没有连上模块，请尝试点击刷新按钮试试.");
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
	
		browser_side_config.net.dip = $("#cfg_text_net_dip input").val();
		browser_side_config.net.dport = parseInt($("#cfg_text_net_dport input").val()) ;
		browser_side_config.net.lport = parseInt($("#cfg_text_net_lport input").val()) ;
	
		browser_side_config.serial.baud = parseInt($("#cfg_text_serial_baud input").val()) ;
		browser_side_config.serial.bits = $("#cfg_select_serial_bits select").val();
		browser_side_config.serial.sbits = $("#cfg_select_serial_sbits select").val();
		browser_side_config.serial.parity = $("#cfg_select_serial_parity select").val();
		browser_side_config.serial.rs485 = $("#cfg_select_serial_rs485 select").val()=="enable"?true:false;

		browser_side_config.holdtime = parseInt($("#cfg_text_other_holdtime input").val()) ;
		browser_side_config.loadmax = parseInt($("#cfg_text_other_loadmax input").val()) ;
	}
	
	function sys_cfg_check_dip(msg)
	{
		var item_val = $("#cfg_text_net_dip input").val();
		
		if(item_val != ""	//可以留空
			 && ip_check(item_val) < 0){
	
			ctrl_grp_add_warning("cfg_text_net_dip", "IP不正确.");
			if(typeof(msg) != "undefined") msg.push("目标IP不正确");
			return -1;
		}
		else{
			ctrl_grp_remove_warning("cfg_text_net_dip","");
			return 0;
		}
	}
	function sys_cfg_check_dport(msg)
	{
		var item_val = $("#cfg_text_net_dport input").val();
		
		if(item_val == "")
			$("#cfg_text_net_dport input").val(0);
	
		if(item_val != "" && //可以留空
			integer_check(item_val) < 0 || parseInt(item_val) > 65535 || parseInt(item_val) < 0){
			ctrl_grp_add_warning("cfg_text_net_dport");
			if(typeof(msg) != "undefined") msg.push("端口设置不正确");
			return -1;
		}
		else{
			ctrl_grp_remove_warning("cfg_text_net_dport","1-65535");
			return 0;
		}
	}
	function sys_cfg_check_lport(msg)
	{
		var item_val = $("#cfg_text_net_lport input").val();
	
		if(integer_check(item_val) < 0 || parseInt(item_val) > 65535 || parseInt(item_val) < 1){
			ctrl_grp_add_warning("cfg_text_net_lport");
			if(typeof(msg) != "undefined") msg.push("本地端口设置不正确");
			return -1;
		}
		else{
			ctrl_grp_remove_warning("cfg_text_net_lport","1-65535");
			return 0;
		}
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
	function sys_cfg_check_holdtime(msg)
	{
		var item_val = $("#cfg_text_other_holdtime input").val();
	
		if(integer_check(item_val) < 0 || parseInt(item_val) > 60000 || parseInt(item_val) < 10){
			ctrl_grp_add_warning("cfg_text_other_holdtime");
			if(typeof(msg) != "undefined") msg.push("holdtime设置不正确");
			return -1;
		}
		else{
			ctrl_grp_remove_warning("cfg_text_other_holdtime","10～60000");
			return 0;
		}
	}
	function sys_cfg_check_loadmax(msg)
	{
		var item_val = $("#cfg_text_other_loadmax input").val();
	
		if(integer_check(item_val) < 0 || parseInt(item_val) > 1460 || parseInt(item_val) < 8){
			ctrl_grp_add_warning("cfg_text_other_loadmax");
			if(typeof(msg) != "undefined") msg.push("loadmax设置不正确");
			return -1;
		}
		else{
			ctrl_grp_remove_warning("cfg_text_other_loadmax","8~1460");
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
	
		flag = ( sys_cfg_check_dip(msg)< 0)? -1:flag;
		flag = ( sys_cfg_check_dport(msg)< 0)? -1:flag;
		flag = ( sys_cfg_check_lport(msg)< 0)? -1:flag;
	
		flag = ( sys_cfg_check_baud(msg)< 0)? -1:flag;
	
		flag = ( sys_cfg_check_holdtime(msg)< 0)? -1:flag;
		flag = ( sys_cfg_check_loadmax(msg)< 0)? -1:flag;
	
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
