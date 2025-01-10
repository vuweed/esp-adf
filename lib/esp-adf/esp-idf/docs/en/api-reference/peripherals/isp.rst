Image Signal Processor
======================

Introduction
------------

{IDF_TARGET_NAME} includes an Image Signal Processor (ISP), which is a feature pipeline that consists of many image processing algorithms. ISP receives image data from the DVP camera or MIPI-CSI camera, or system memory, and writes the processed image data to the system memory through DMA. ISP shall work with other modules to read and write data, it can not work alone.

Terminology
-----------

.. list::
  - MIPI-CSI: Camera serial interface, a high-speed serial interface for cameras compliant with MIPI specifications
  - DVP: Digital video parallel interface, generally composed of vsync, hsync, de, and data signals
  - RAW: Unprocessed data directly output from an image sensor, typically divided into R, Gr, Gb, and B four channels classified into RAW8, RAW10, RAW12, etc., based on bit width
  - RGB: Colored image format composed of red, green, and blue colors classified into RGB888, RGB565, etc., based on the bit width of each color
  - YUV: Colored image format composed of luminance and chrominance classified into YUV444, YUV422, YUV420, etc., based on the data arrangement
  - BF:  Bayer Domain Filter
  - AF:  Auto-focus
  - AWB: Auto-white balance
  - CCM: Color correction matrix

ISP Pipeline
------------

.. blockdiag::
    :scale: 100%
    :caption: ISP Pipeline
    :align: center

    blockdiag isp_pipeline {
        orientation = portrait;
        node_height = 30;
        node_width = 120;
        span_width = 100;
        default_fontsize = 16;

        isp_header [label = "ISP Header"];
        isp_tail [label = "ISP Tail"];
        isp_chs [label = "Contrast &\n Hue & Saturation", width = 150, height = 70];
        isp_yuv [label = "YUV Limit\nYUB2RGB", width = 120, height = 70];

        isp_header -> BF -> Demosaic -> CCM -> RGB2YUV -> isp_chs -> isp_yuv -> isp_tail;

        BF -> HIST
        Demosaic -> AWB
        Demosaic -> AE
        Demosaic -> HIST
        CCM -> AWB
        CCM -> AE
        RGB2YUV -> HIST
        RGB2YUV -> AF
    }

Functional Overview
-------------------

The ISP driver offers following services:

-  `Resource Allocation <#isp-resource-allocation>`__ - covers how to allocate ISP resources with properly set of configurations. It also covers how to recycle the resources when they finished working.
-  `Enable and disable ISP processor <#isp-enable-disable>`__ - covers how to enable and disable an ISP processor.
-  `Get AF statistics in one shot or continuous way <#isp-af-statistics>`__ - covers how to get AF statistics one-shot or continuously.
-  `Get AWB statistics in one shot or continuous way <#isp-awb-statistics>`__ - covers how to get AWB white patches statistics one-shot or continuously.
-  `Configure CCM <#isp-ccm-config>`__ - covers how to config the Color Correction Matrix.
-  `Register callback <#isp-callback>`__ - covers how to hook user specific code to ISP driver event callback function.
-  `Thread Safety <#isp-thread-safety>`__ - lists which APIs are guaranteed to be thread safe by the driver.
-  `Kconfig Options <#isp-kconfig-options>`__ - lists the supported Kconfig options that can bring different effects to the driver.
-  `IRAM SAFE <#isp-iram-safe>`__ - describes tips on how to make the ISP interrupt and control functions work better along with a disabled cache.

.. _isp-resource-allocation:

Resource Allocation
^^^^^^^^^^^^^^^^^^^

Install ISP Driver
~~~~~~~~~~~~~~~~~~

ISP driver requires the configuration that specified by :cpp:type:`esp_isp_processor_cfg_t`.

If the configurations in :cpp:type:`esp_isp_processor_cfg_t` is specified, users can call :cpp:func:`esp_isp_new_processor` to allocate and initialize an ISP processor. This function will return an ISP processor handle if it runs correctly. You can take following code as reference.

.. code:: c

    esp_isp_processor_cfg_t isp_config = {
        .clk_src = ISP_CLK_SRC_DEFAULT,
        .clk_hz = 80 * 1000 * 1000,
        .input_data_source = ISP_INPUT_DATA_SOURCE_CSI,
        .input_data_color_type = ISP_COLOR_RAW8,
        .output_data_color_type = ISP_COLOR_RGB565,
    };

    isp_proc_handle_t isp_proc = NULL;
    ESP_ERROR_CHECK(esp_isp_new_processor(&isp_config, &isp_proc));

You can use the created handle to do driver enable / disable the ISP driver and do other ISP module installation.


Install ISP Auto-Focus (AF) Driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

ISP auto-focus (AF) driver requires the configuration that specified by :cpp:type:`esp_isp_af_config_t`.

If the configurations in :cpp:type:`esp_isp_af_config_t` is specified, users can call :cpp:func:`esp_isp_new_af_controller` to allocate and initialize an ISP AF processor. This function will return an ISP AF processor handle if it runs correctly. You can take following code as reference.

.. code:: c

    esp_isp_af_config_t af_config = {
        .edge_thresh = 128,
    };
    isp_af_ctlr_t af_ctrlr = NULL;
    ESP_ERROR_CHECK(esp_isp_new_af_controller(isp_proc, &af_config, &af_ctrlr));

You can use the created handle to do driver enable / disable the ISP AF driver and ISP AF Env module installation.

Install ISP Auto-White-Balance (AWB) Driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

ISP auto-white-balance (AWB) driver requires the configuration specified by :cpp:type:`esp_isp_awb_config_t`.

If an :cpp:type:`esp_isp_awb_config_t` configuration is specified, you can call :cpp:func:`esp_isp_new_awb_controller` to allocate and initialize an ISP AWB processor. This function will return an ISP AWB processor handle on success. You can take following code as reference.

.. code:: c

    isp_awb_ctlr_t awb_ctlr = NULL;
    uint32_t image_width = 800;
    uint32_t image_height = 600;
    /* The AWB configuration, please refer to the API comment for how to tune these parameters */
    esp_isp_awb_config_t awb_config = {
        .sample_point = ISP_AWB_SAMPLE_POINT_AFTER_CCM,
        .window = {
            .top_left = {.x = image_width * 0.2, .y = image_height * 0.2},
            .btm_right = {.x = image_width * 0.8, .y = image_height * 0.8},
        },
        .white_patch = {
            .luminance = {.min = 0, .max = 220 * 3},
            .red_green_ratio = {.min = 0.0f, .max = 3.999f},
            .blue_green_ratio = {.min = 0.0f, .max = 3.999f},
        },
    };
    ESP_ERROR_CHECK(esp_isp_new_awb_controller(isp_proc, &awb_config, &awb_ctlr));

The AWB handle created in this step is required by other AWB APIs and AWB scheme.

Uninstall ISP Driver
~~~~~~~~~~~~~~~~~~~~

If a previously installed ISP processor is no longer needed, it's recommended to recycle the resource by calling :cpp:func:`esp_isp_del_processor`, so that to release the underlying hardware.

UnInstall ISP AF Driver
~~~~~~~~~~~~~~~~~~~~~~~

If a previously installed ISP AF processor is no longer needed, it's recommended to recycle the resource by calling :cpp:func:`esp_isp_del_af_controller`, so that to release the underlying hardware.

UnInstall ISP AWB Driver
~~~~~~~~~~~~~~~~~~~~~~~~

If a previously installed ISP AWB processor is no longer needed, it's recommended to free the resource by calling :cpp:func:`esp_isp_del_awb_controller`, it will also release the underlying hardware.


.. _isp-enable-disable:

Enable and Disable ISP
^^^^^^^^^^^^^^^^^^^^^^

ISP
---------
Before doing ISP pipeline, you need to enable the ISP processor first, by calling :cpp:func:`esp_isp_enable`. This function:

* Switches the driver state from **init** to **enable**.

Calling :cpp:func:`esp_isp_disable` does the opposite, that is, put the driver back to the **init** state.

ISP AF Processor
----------------

Before doing ISP AF, you need to enable the ISP AF processor first, by calling :cpp:func:`esp_isp_af_controller_enable`. This function:

* Switches the driver state from **init** to **enable**.

Calling :cpp:func:`esp_isp_af_controller_disable` does the opposite, that is, put the driver back to the **init** state.

.. _isp-af-statistics:

AF One-shot and Continuous Statistics
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Calling :cpp:func:`esp_isp_af_controller_get_oneshot_statistics` to get oneshot AF statistics result. You can take following code as reference.

Aside from the above oneshot API, the ISP AF driver also provides a way to start AF statistics continuously. Calling :cpp:func:`esp_isp_af_controller_start_continuous_statistics` to start the continuous statistics and :cpp:func:`esp_isp_af_controller_stop_continuous_statistics` to stop it.

Note that if you want to use the continuous statistics, you need to register the :cpp:member:`esp_isp_af_env_detector_evt_cbs_t::on_env_statistics_done` or :cpp:member:`esp_isp_af_env_detector_evt_cbs_t::on_env_change` callback to get the statistics result. See how to register in `Register Event Callbacks <#isp-callback>`__

.. code:: c

    esp_isp_af_config_t af_config = {
        .edge_thresh = 128,
    };
    isp_af_ctlr_t af_ctrlr = NULL;
    ESP_ERROR_CHECK(esp_isp_new_af_controller(isp_proc, &af_config, &af_ctrlr));
    ESP_ERROR_CHECK(esp_isp_af_controller_enable(af_ctrlr));
    isp_af_result_t result = {};
    /* Trigger the AF statistics and get its result for one time with timeout value 2000ms. */
    ESP_ERROR_CHECK(esp_isp_af_controller_get_oneshot_statistics(af_ctrlr, 2000, &result));

    /* Start continuous AF statistics */
    ESP_ERROR_CHECK(esp_isp_af_controller_start_continuous_statistics(af_ctrlr));
    // You can do other stuffs here, the statistics result can be obtained in the callback
    // ......
    // vTaskDelay(pdMS_TO_TICKS(1000));
    /* Stop continuous AF statistics */
    ESP_ERROR_CHECK(esp_isp_af_controller_stop_continuous_statistics(af_ctrlr));

    /* Disable the af controller */
    ESP_ERROR_CHECK(esp_isp_af_controller_disable(af_ctrlr));
    /* Delete the af controller and free the resources */
    ESP_ERROR_CHECK(esp_isp_del_af_controller(af_ctrlr));

Set AF Environment Detector
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Calling :cpp:func:`esp_isp_af_controller_set_env_detector` to set an ISP AF environment detector. You can take following code as reference.

.. code:: c

    esp_isp_af_env_config_t env_config = {
        .interval = 10,
    };
    isp_af_ctlr_t af_ctrlr = NULL;
    ESP_ERROR_CHECK(esp_isp_new_af_controller(isp_proc, &af_config, &af_ctrlr));
    ESP_ERROR_CHECK(esp_isp_af_controller_set_env_detector(af_ctrlr, &env_config));

Set AF Environment Detector Threshold
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Calling :cpp:func:`esp_isp_af_env_detector_set_threshold` to set the threshold of an ISP AF environment detector.

.. code:: c

    int definition_thresh = 0;
    int luminance_thresh = 0;
    ESP_ERROR_CHECK(esp_isp_af_env_detector_set_threshold(env_detector, definition_thresh, luminance_thresh));

ISP AWB Processor
-----------------

Before doing ISP AWB, you need to enable the ISP AWB processor first, by calling :cpp:func:`esp_isp_awb_controller_enable`. This function:

* Switches the driver state from **init** to **enable**.

Calling :cpp:func:`esp_isp_awb_controller_disable` does the opposite, that is, put the driver back to the **init** state.

.. _isp-awb-statistics:

AWB One-shot and Continuous Statistics
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Calling :cpp:func:`esp_isp_awb_controller_get_oneshot_result` to get oneshot AWB statistics result of white patches. You can take following code as reference.

Aside from the above oneshot API, the ISP AWB driver also provides a way to start AWB statistics continuously. Calling :cpp:func:`esp_isp_awb_controller_start_continuous_statistics` starts the continuous statistics and :cpp:func:`esp_isp_awb_controller_stop_continuous_statistics` stops it.

Note that if you want to use the continuous statistics, you need to register the :cpp:member:`esp_isp_awb_cbs_t::on_statistics_done` callback to get the statistics result. See how to register it in `Register Event Callbacks <#isp-callback>`__

.. code:: c

    bool example_isp_awb_on_statistics_done_cb(isp_awb_ctlr_t awb_ctlr, const esp_isp_awb_evt_data_t *edata, void *user_data);
    // ...
    isp_awb_ctlr_t awb_ctlr = NULL;
    uint32_t image_width = 800;
    uint32_t image_height = 600;
    /* The AWB configuration, please refer to the API comment for how to tune these parameters */
    esp_isp_awb_config_t awb_config = {
        .sample_point = ISP_AWB_SAMPLE_POINT_AFTER_CCM,
        .window = {
            .top_left = {.x = image_width * 0.2, .y = image_height * 0.2},
            .btm_right = {.x = image_width * 0.8, .y = image_height * 0.8},
        },
        .white_patch = {
            .luminance = {.min = 0, .max = 220 * 3},
            .red_green_ratio = {.min = 0.0f, .max = 3.999f},
            .blue_green_ratio = {.min = 0.0f, .max = 3.999f},
        },
    };
    isp_awb_stat_result_t stat_res = {};
    /* Create the awb controller */
    ESP_ERROR_CHECK(esp_isp_new_awb_controller(isp_proc, &awb_config, &awb_ctlr));
    /* Register AWB callback */
    esp_isp_awb_cbs_t awb_cb = {
        .on_statistics_done = example_isp_awb_on_statistics_done_cb,
    };
    ESP_ERROR_CHECK(esp_isp_awb_register_event_callbacks(awb_ctlr, &awb_cb, NULL));
    /* Enabled the awb controller */
    ESP_ERROR_CHECK(esp_isp_awb_controller_enable(awb_ctlr));

    /* Get oneshot AWB statistics result */
    ESP_ERROR_CHECK(esp_isp_awb_controller_get_oneshot_statistics(awb_ctlr, -1, &stat_res));

    /* Start continuous AWB statistics, note that continuous statistics requires `on_statistics_done` callback */
    ESP_ERROR_CHECK(esp_isp_awb_controller_start_continuous_statistics(awb_ctlr));
    // You can do other stuffs here, the statistics result can be obtained in the callback
    // ......
    // vTaskDelay(pdMS_TO_TICKS(1000));
    /* Stop continuous AWB statistics */
    ESP_ERROR_CHECK(esp_isp_awb_controller_stop_continuous_statistics(awb_ctlr));

    /* Disable the awb controller */
    ESP_ERROR_CHECK(esp_isp_awb_controller_disable(awb_ctlr));
    /* Delete the awb controller and free the resources */
    ESP_ERROR_CHECK(esp_isp_del_awb_controller(awb_ctlr));

.. _isp-ccm-config:

Configure CCM
^^^^^^^^^^^^^

Color Correction Matrix can scale the color ratio of RGB888 pixels. It can be used for adjusting the image color via some algorithms, for example, used for white balance by inputting the AWB computed result, or used as a Filter with some filter algorithms.

To adjust the color correction matrix, you can refer to the following code:

.. code-block:: c

    // ...
    // Configure CCM
    esp_isp_ccm_config_t ccm_cfg = {
        .matrix = {
            1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0
        },
        .saturation = false,
    };
    ESP_ERROR_CHECK(esp_isp_ccm_configure(isp_proc, &ccm_cfg));
    // The configured CCM will be applied to the image once the CCM module is enabled
    ESP_ERROR_CHECK(esp_isp_ccm_enable(isp_proc));
    // CCM can also be configured after it is enabled
    ccm_cfg.matrix[0][0] = 2.0;
    ESP_ERROR_CHECK(esp_isp_ccm_configure(isp_proc, &ccm_cfg));
    // Disable CCM if no longer needed
    ESP_ERROR_CHECK(esp_isp_ccm_disable(isp_proc));

.. _isp-callback:

Register Event Callbacks
^^^^^^^^^^^^^^^^^^^^^^^^

Register ISP AF Environment Detector Event Callbacks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After the ISP AF environment detector starts up, it can generate a specific event dynamically. If you have some functions that should be called when the event happens, please hook your function to the interrupt service routine by calling :cpp:func:`esp_isp_af_env_detector_register_event_callbacks`. All supported event callbacks are listed in :cpp:type:`esp_isp_af_env_detector_evt_cbs_t`:

-  :cpp:member:`esp_isp_af_env_detector_evt_cbs_t::on_env_statistics_done` sets a callback function for environment statistics done. As this function is called within the ISR context, you must ensure that the function does not attempt to block (e.g., by making sure that only FreeRTOS APIs with ``ISR`` suffix are called from within the function). The function prototype is declared in :cpp:type:`esp_isp_af_env_detector_callback_t`.
-  :cpp:member:`esp_isp_af_env_detector_evt_cbs_t::on_env_change` sets a callback function for environment change. As this function is called within the ISR context, you must ensure that the function does not attempt to block (e.g., by making sure that only FreeRTOS APIs with ``ISR`` suffix are called from within the function). The function prototype is declared in :cpp:type:`esp_isp_af_env_detector_callback_t`.

You can save your own context to :cpp:func:`esp_isp_af_env_detector_register_event_callbacks` as well, via the parameter ``user_data``. The user data will be directly passed to the callback function.

Register ISP AWB Statistics Done Event Callbacks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After the ISP AWB controller finished statistics of white patches, it can generate a specific event dynamically. If you want to be informed when the statistics done event takes place, please hook your function to the interrupt service routine by calling :cpp:func:`esp_isp_awb_register_event_callbacks`. All supported event callbacks are listed in :cpp:type:`esp_isp_awb_cbs_t`:

-  :cpp:member:`esp_isp_awb_cbs_t::on_statistics_done` sets a callback function when finished statistics of the white patches. As this function is called within the ISR context, you must ensure that the function does not attempt to block (e.g., by making sure that only FreeRTOS APIs with ``ISR`` suffix are called from within the function). The function prototype is declared in :cpp:type:`esp_isp_awb_callback_t`.

You can save your own context via the parameter ``user_data`` of :cpp:func:`esp_isp_awb_register_event_callbacks`. The user data will be directly passed to the callback function.

.. _isp-thread-safety:

Thread Safety
^^^^^^^^^^^^^

The factory function :cpp:func:`esp_isp_new_processor`, :cpp:func:`esp_isp_del_processor`, :cpp:func:`esp_isp_new_af_controller`, :cpp:func:`esp_isp_del_af_controller`, :cpp:func:`esp_isp_new_af_env_detector`, and :cpp:func:`esp_isp_del_af_env_detector` are guaranteed to be thread safe by the driver, which means, user can call them from different RTOS tasks without protection by extra locks.

.. _isp-kconfig-options:

Kconfig Options
^^^^^^^^^^^^^^^

- :ref:`CONFIG_ISP_ISR_IRAM_SAFE` controls whether the default ISR handler should be masked when the cache is disabled

.. _isp-iram-safe:

IRAM Safe
^^^^^^^^^

By default, the ISP interrupt will be deferred when the cache is disabled because of writing or erasing the flash.

There is a Kconfig option :ref:`CONFIG_ISP_ISR_IRAM_SAFE` that:

-  Enables the interrupt being serviced even when the cache is disabled
-  Places all functions that used by the ISR into IRAM
-  Places driver object into DRAM (in case it is mapped to PSRAM by accident)

This allows the interrupt to run while the cache is disabled, but comes at the cost of increased IRAM consumption.

API Reference
-------------

.. include-build-file:: inc/isp.inc
.. include-build-file:: inc/isp_types.inc
.. include-build-file:: inc/isp_af.inc
