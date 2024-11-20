package com.example.h264decoder;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;
import android.widget.EditText;
import android.widget.Button;
import android.widget.Toast;
import com.example.h264decoder.R;


public class MainActivity extends AppCompatActivity {

    private static final String TAG = "H264Decoder";
    // Used to load the 'h264coder' library on application startup.
    static {
        System.loadLibrary("h264decoder");
    }

    public native int decode(String inputPath,String outputPath);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        EditText inputPathEditText = findViewById(R.id.inputPath);
        EditText outputPathEditText = findViewById(R.id.outputPath);
        Button decodeButton = findViewById(R.id.decodeButton);

        decodeButton.setOnClickListener(v -> {
            // Get user input paths
            String inputPath = inputPathEditText.getText().toString().trim();
            String outputPath = outputPathEditText.getText().toString().trim();

            // Validate paths
            if (inputPath.isEmpty() || outputPath.isEmpty()) {
                Toast.makeText(MainActivity.this, "Please provide both input and output paths.", Toast.LENGTH_SHORT).show();
                return;
            }

//        String inputPath = "/data/local/tmp/output_40.h264";
//        String outputPath = "/data/local/tmp/output.yuv";

            int result = decode(inputPath, outputPath);
            if (result == 0) {
                Log.d(TAG, "Decoding completed successfully.");
            } else {
                Log.e(TAG, "Decoding failed with error code: " + result);
            }
        });
    }
}