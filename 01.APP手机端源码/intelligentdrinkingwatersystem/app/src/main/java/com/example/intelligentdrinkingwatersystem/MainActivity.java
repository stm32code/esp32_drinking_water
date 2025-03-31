package com.example.intelligentdrinkingwatersystem;

import static com.example.intelligentdrinkingwatersystem.utils.Common.PushTopic;
import static com.google.zxing.integration.android.IntentIntegrator.REQUEST_CODE;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Toast;

import com.blankj.utilcode.util.LogUtils;
import com.example.intelligentdrinkingwatersystem.bean.Receive;
import com.example.intelligentdrinkingwatersystem.bean.Send;
import com.example.intelligentdrinkingwatersystem.databinding.ActivityMainBinding;
import com.example.intelligentdrinkingwatersystem.utils.Common;
import com.example.intelligentdrinkingwatersystem.utils.MToast;
import com.google.gson.Gson;
import com.gyf.immersionbar.ImmersionBar;
import com.itfitness.mqttlibrary.MQTTHelper;
import com.journeyapps.barcodescanner.CaptureActivity;

import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.greenrobot.eventbus.EventBus;

import java.util.ArrayList;
import java.util.List;
import java.util.TimeZone;

import pub.devrel.easypermissions.EasyPermissions;

public class MainActivity extends AppCompatActivity {
    private ActivityMainBinding binding;
    private boolean isScanFlag = false; //是否扫码

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
        getPermission();
        initView();
        mqttConfig();
    }

    /***
     * mqtt配置
     */
    private void mqttConfig() {
        Common.mqttHelper = new MQTTHelper(this, Common.Sever, Common.DriveID, Common.DriveName, Common.DrivePassword, true, 30, 60);

        try {
            Common.mqttHelper.connect(Common.ReceiveTopic, 1, true, new MqttCallback() {
                @Override
                public void connectionLost(Throwable cause) {

                }

                @Override
                public void messageArrived(String topic, MqttMessage message) {
                    //收到消息
                    Receive data = new Gson().fromJson(message.toString(), Receive.class);
                    LogUtils.eTag("接收到消息", message.getPayload() != null ? new String(message.getPayload()) : "");
                    System.out.println(data);
                    dataAnalysis(data);
                }

                @Override
                public void deliveryComplete(IMqttDeliveryToken token) {

                }
            });
        } catch (MqttException e) {
            e.printStackTrace();
            MToast.mToast(this, "MQTT连接错误");
        }
    }

    
    /***
     * 数据分析
     * @param data
     */
    private void dataAnalysis(Receive data) {
       
            if (data.getWater_n() != null) { //水流量
                float water = Integer.parseInt(data.getWater_n());
                binding.waterFlowText.setText(data.getWater_n());
                binding.waterRateText.setText(String.format("%.2f", water * 0.01));
            }

        } catch (Exception e) {
            e.printStackTrace();
            Log.e("接收数据", e.toString());
            MToast.mToast(this, "数据解析失败");
        }
    }

    private void initView() {
        TimeZone.setDefault(TimeZone.getTimeZone("Etc/GMT-8"));
        ImmersionBar.with(this).init();
        setSupportActionBar(binding.toolbar);
        binding.toolbarLayout.setTitle(getTitle());
        initWaterState();
        eventManager();
    }

    private void eventManager() {
        binding.waterOpenBtn.setOnClickListener(view -> {
            if (Common.mqttHelper != null && Common.mqttHelper.getConnected()) {
                binding.waterOpenBtn.setSelected(!binding.waterOpenBtn.isSelected());
                sendMessage(1, binding.waterOpenBtn.isSelected() ? "1" : "0");
            } else {
                MToast.mToast(this, "未连接");
            }
        });

        binding.scanQR.setOnClickListener(view -> {
            if (Common.mqttHelper != null && Common.mqttHelper.getConnected()) {
                if (!isScanFlag && binding.scanQRText.getText().equals("扫码接水")) {
                    //跳转扫码界面
                    Intent intent = new Intent(this, ScanActivity.class);
                    launcher.launch(intent);
                } else {
                    String w = binding.waterRateText.getText().toString();
                    if (w.isEmpty() || w.equals("wait")) {
                        initWaterState();
                        return;
                    }
                    //跳转付款界面
                    Intent intent = new Intent(this, PayActivity.class);
                    intent.putExtra("pay", w);
                    launcher.launch(intent);
                }
            } else {
                MToast.mToast(this, "未连接");
            }
        });
    }

    // 处理扫描界面返回的信息
    ActivityResultLauncher<Intent> launcher = registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
        if (result.getResultCode() == RESULT_OK) {
            Log.e("返回", "ActivityResult");
            Intent intent = result.getData();
            if (intent != null && intent.hasExtra("result")) { //扫码返回
                //返回的文本内容
                String content = intent.getStringExtra("result");
                binding.deviceText.setText(content);
                isScanFlag = true;
                binding.waterNumberLayout.setVisibility(View.VISIBLE);
                binding.scanQRText.setText("结束付款");
                binding.layout1.setVisibility(View.VISIBLE);
                binding.scanQRImage.setVisibility(View.GONE);
                binding.scanQR.setBackground(getDrawable(R.drawable.scanqr_button_background_0));
                Log.e("扫码反馈测试", content);
                sendMessage(2, "1");
//                Toast.makeText(this, content, Toast.LENGTH_SHORT).show();
                return;
            }
            if (intent != null && intent.hasExtra("resultPay")) { //付款返回
                initWaterState();
            }
        }
    });

    /**
     * @param message 需要发送的消息
     * @brief 再次封装MQTT发送
     */
    private void sendMessage(int cmd, String... message) {
        if (Common.mqttHelper != null && Common.mqttHelper.getConnected()) {
            Send send = new Send();
            switch (cmd) {
                case 1:
                    send.setPump(Integer.parseInt(message[0]));
                    break;
                case 2:
                    send.setState(Integer.parseInt(message[0]));
                    break;
            }
            send.setCmd(cmd);
            String str = new Gson().toJson(send);
            new Thread(() -> Common.mqttHelper.publish(PushTopic, str, 1)).start();
        }
    }

    /**
     * @brief 动态获取权限
     */
    private void getPermission() {
        List<String> perms = new ArrayList<>();
        perms.add(Manifest.permission.INTERNET);
        perms.add(Manifest.permission.VIBRATE);
        perms.add(Manifest.permission.CAMERA);
        if (!EasyPermissions.hasPermissions(this, perms.toArray(new String[0]))) {
            //请求权限
            EasyPermissions.requestPermissions(this, "这是必要的权限", 100, perms.toArray(new String[0]));
        }
        if (!EasyPermissions.hasPermissions(this, perms.toArray(new String[0]))) {
            //请求权限
            EasyPermissions.requestPermissions(this, "这是必要的权限", 100, perms.toArray(new String[0]));
        } else {
        }

    }
}