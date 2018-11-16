
#define INPUT_DEV_COUNT 2

struct MTK_OVL_ADAPTER_BUF_ADDR {
	unsigned long addr_va;
	unsigned int index;
};

struct MTK_OVL_ADAPTER_BUF_INFO {
	unsigned int enable;
	unsigned int stream_on;
	unsigned int pitch;
	unsigned int bytesperline;
	unsigned int width;
	unsigned int height;
	unsigned int fmt_4cc;
	unsigned int buf_cnt;
	unsigned int plane_cnt;

	unsigned int mem_type; //enum v4l2_memory
	unsigned int buf_type; //enum v4l2_buf_type

	struct timeval timestamp;
};

struct MTK_OVL_ADAPTER_AREA_INFO {
	unsigned int width;
	unsigned int height;
};

struct MTK_OVL_ADAPTER_COORDINATE_INFO {
	unsigned int left;
	unsigned int top;
};

struct MTK_OVL_ADAPTER_PARAM_INIT {
	struct MTK_OVL_ADAPTER_BUF_INFO input_buf[INPUT_DEV_COUNT];
	struct MTK_OVL_ADAPTER_BUF_INFO output_buf;
};

struct MTK_OVL_ADAPTER_PARAM_UNINIT {

};

struct MTK_OVL_ADAPTER_PARAM_WORK {
	struct MTK_OVL_ADAPTER_BUF_ADDR input_addr[INPUT_DEV_COUNT];
	struct MTK_OVL_ADAPTER_BUF_ADDR output_addr;

	struct MTK_OVL_ADAPTER_AREA_INFO transition_area;
	struct MTK_OVL_ADAPTER_AREA_INFO input_area[INPUT_DEV_COUNT];
	struct MTK_OVL_ADAPTER_COORDINATE_INFO input_coordinate[INPUT_DEV_COUNT];
	struct MTK_OVL_ADAPTER_COORDINATE_INFO transition_coordinate[INPUT_DEV_COUNT];
	struct MTK_OVL_ADAPTER_AREA_INFO output_area;
	struct MTK_OVL_ADAPTER_COORDINATE_INFO output_coordinate;
};

struct MTK_OVL_ADAPTER_API{
	int(*pfn_mtk_ovl_adapter_init)(void **p_handle, struct MTK_OVL_ADAPTER_PARAM_INIT *param);

	int(*pfn_mtk_ovl_adapter_uninit)(void *handle);

	int(*pfn_mtk_ovl_adapter_work)(void *handle, struct MTK_OVL_ADAPTER_PARAM_WORK *param);
};

