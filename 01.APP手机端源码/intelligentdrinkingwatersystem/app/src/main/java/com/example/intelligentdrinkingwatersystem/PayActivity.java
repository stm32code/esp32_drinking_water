package com.example.intelligentdrinkingwatersystem;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;

import com.example.intelligentdrinkingwatersystem.databinding.ActivityPayBinding;
import com.example.intelligentdrinkingwatersystem.utils.HandlerAction;
import com.example.intelligentdrinkingwatersystem.utils.MToast;

public class PayActivity extends AppCompatActivity implements HandlerAction {
    private ActivityPayBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityPayBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        initView();
    }

    private void initView() {
        String payMoney = getIntent().getStringExtra("pay");
        if (payMoney != null) {
            binding.moneyText.setText(payMoney);
        } else {
            binding.moneyText.setText("1.00");
        }

        binding.payBtn.setOnClickListener(view -> {
            MToast.mToast(this, "付款成功");
            postDelayed(() -> {
                Intent intent = new Intent();
                intent.putExtra("resultPay", "ok");
                setResult(RESULT_OK, intent);
                finish();
            }, 1000);
        });

    }
}