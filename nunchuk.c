//
// Use evtest application to work with nunchuk device from userspace
//

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input-polldev.h>

#define NUNCHUK_REGS_NUMBER	6
#define NUNCHUK_FIRST_REG_ADDR	0x00
#define NUNCHUK_POLL_INTERVAL	50

struct nunchuk_dev {
	struct i2c_client *i2c_client;
};


static int nunchuk_read_registers(struct i2c_client *client, char *buf)
{
	int count;
	char first_reg_addr = NUNCHUK_FIRST_REG_ADDR;

	msleep(10);

	count = i2c_master_send(client, &first_reg_addr, 1);
	if (count != 1)
		return -EIO;

	msleep(10);
	
	count = i2c_master_recv(client, buf, NUNCHUK_REGS_NUMBER);
	if (count != NUNCHUK_REGS_NUMBER)
		return -EIO;
	
	return 0;
}

static void nunchuk_poll(struct input_polled_dev *dev)
{
	struct i2c_client *i2c_client;
	char regs_buf[NUNCHUK_REGS_NUMBER];
	unsigned int zpressed, cpressed;

	i2c_client = ((struct nunchuk_dev *)dev->private)->i2c_client;

	if(nunchuk_read_registers(i2c_client, regs_buf)) {
		pr_info("Reading of registers has failed");
		return;
	}
	zpressed = ~regs_buf[5] & 1;
	cpressed = (~regs_buf[5] >> 1) & 1;

	input_event(dev->input, EV_KEY, BTN_Z, zpressed);
	input_event(dev->input, EV_KEY, BTN_C, cpressed);
	input_sync(dev->input);
}


static int nunchuk_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	char handshake_1[] = {0xF0, 0x55};
	char handshake_2[] = {0xFB, 0x00};
	int count, error;
	struct input_polled_dev *polled_input;
	struct input_dev *input;
	struct nunchuk_dev *nunchuk;

	nunchuk = devm_kzalloc(&client->dev, sizeof(struct nunchuk_dev), GFP_KERNEL);
	if (!nunchuk) {
		return -ENOMEM;
	}
	nunchuk->i2c_client = client;

	count = i2c_master_send(client, handshake_1, sizeof(handshake_1));
	if (count != sizeof(handshake_1)) {
		pr_info("Handshake part 1 has failed!\n");
		return -EIO;
	}
	msleep(1);
	count = i2c_master_send(client, handshake_2, sizeof(handshake_2));
	if (count != sizeof(handshake_2)) {
		pr_info("Handshake has part 2 failed!\n");
		return -EIO;
	}

	polled_input = devm_input_allocate_polled_device(&client->dev);
	if (!polled_input)
		return -ENOMEM;

	input = polled_input->input;
	input->name = "Wii Nunchuk";
	input->id.bustype = BUS_I2C;
	polled_input->private = nunchuk;
	polled_input->poll = nunchuk_poll;
	polled_input->poll_interval = NUNCHUK_POLL_INTERVAL;

	set_bit(EV_KEY, input->evbit);
	set_bit(BTN_C, input->keybit);
	set_bit(BTN_Z, input->keybit);

	error = input_register_polled_device(polled_input);
	if (error)
		return error;

	return 0;
}

static int nunchuk_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct of_device_id nunchuk_of_match[] = {
	{ .compatible = "nintendo,nunchuk" },
	{ },
};

MODULE_DEVICE_TABLE(of, nunchuk_of_match);

static struct i2c_driver nunchuk_i2c_driver = {
	.driver = {
		.name = "nunchuk_i2c",
		.of_match_table = nunchuk_of_match,
	},
	.probe = nunchuk_i2c_probe,
	.remove = nunchuk_i2c_remove,
};

module_i2c_driver(nunchuk_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Max Kokhan");
