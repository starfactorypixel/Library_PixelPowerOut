#pragma once
#include <inttypes.h>
#include <string.h>

template <uint8_t _ports_max, uint16_t _tick_time = 10> 
class PowerOutV2
{
	//static constexpr uint8_t _ports_max = 8;
	//static constexpr uint16_t _tick_time = 10;
	
	using tick_provider = uint32_t (*)();
	using callback_control_t = void (*)(uint8_t id, uint8_t state);
	using callback_current_t = uint16_t (*)(uint8_t id);
	using callback_current_limit_t = void (*)(uint8_t port, uint16_t current);
	
	public:
		
		enum mode_t : uint8_t { MODE_NONE, MODE_OFF, MODE_ON, MODE_PWM, MODE_BLINK, MODE_DELAY_OFF };
		enum state_t : uint8_t { STATE_NONE, STATE_OFF, STATE_ON };
		
		PowerOutV2(tick_provider tick, callback_control_t control, callback_current_t current) : _Tick(tick), _CallbackControl(control), _CallbackCurrent(current)
		{
			memset(_channels, 0x00, sizeof(_channels));
			
			return;
		}
		
		// Добавить порт в библиотеку
		// uint8_t port - Номер порта, начиная с 1
		// uint8_t control_id - ID порта управления
		// uint8_t current_id - ID порта чтения ADC
		// uint16_t current_limit - Лимит тока в мА
		void SetPort(uint8_t port, uint8_t control_id, uint8_t current_id, uint16_t current_limit)
		{
			if(--port >= _ports_max) return;
			
			channel_t &channel = _channels[port];
			channel.control_id = control_id;
			channel.current_id = current_id;
			channel.current_limit = current_limit;
			channel.mode = MODE_OFF;
			_CallControl(channel, STATE_OFF);
			
			return;
		}
		
		void Init()
		{
			return;
		}
		
		void SetCallbackCurrentLimit(callback_current_limit_t callback)
		{
			_CallbackCurrentLimit = callback;
			
			return;
		}
		
		// Включить порт
		// uint8_t port - Номер порта, начиная с 1
		void CtrlOn(uint8_t port)
		{
			if(--port >= _ports_max) return;
			
			channel_t &channel = _channels[port];
			_CallControl(channel, STATE_ON);
			channel.mode = MODE_ON;
			
			return;
		}
		
		// Включить порт на указанное время
		// uint8_t port - Номер порта, начиная с 1
		// uint16_t time - Время работы порта
		void CtrlOn(uint8_t port, uint16_t time)
		{
			if(--port >= _ports_max) return;
			
			channel_t &channel = _channels[port];
			_CallControl(channel, STATE_ON);
			channel.mode = MODE_DELAY_OFF;
			channel.off_delay = time;
			channel.last_time = _Tick();
			
			return;
		}
		
		// Включить порт в режиме моргания
		// uint8_t port - Номер порта, начиная с 1
		// uint16_t blink_on - Время включенного состояния, мс
		// uint16_t blink_off - Время выключенного состояния, мс
		void CtrlOn(uint8_t port, uint16_t blink_on, uint16_t blink_off)
		{
			if(--port >= _ports_max) return;
			
			channel_t &channel = _channels[port];
			_CallControl(channel, STATE_ON);
			channel.mode = MODE_BLINK;
			channel.blink_on = blink_on;
			channel.blink_off = blink_off;
			
			// Исправляем 'промигивание' при включении.
			channel.blink_delay = blink_on;
			channel.last_time = _Tick();
			
			return;
		}
		
		// Выключить порт
		// uint8_t port - Номер порта, начиная с 1
		void CtrlOff(uint8_t port)
		{
			if(--port >= _ports_max) return;
			
			channel_t &channel = _channels[port];
			_CallControl(channel, STATE_OFF);
			channel.mode = MODE_OFF;
			
			return;
		}
		
		// Выключить порт через указанное время
		// uint8_t port - Номер порта, начиная с 1
		// uint16_t time - Время работы порта
		void CtrlOff(uint8_t port, uint16_t time)
		{
			if(--port >= _ports_max) return;
			
			channel_t &channel = _channels[port];
			channel.mode = MODE_DELAY_OFF;
			channel.off_delay = time;
			channel.last_time = _Tick();
			
			return;
		}
		
		// Переключить порт
		// uint8_t port - Номер порта, начиная с 1
		void CtrlToggle(uint8_t port)
		{
			if(--port >= _ports_max) return;
			
			channel_t &channel = _channels[port];
			switch(channel.mode)
			{
				case MODE_ON:
				case MODE_PWM:
				case MODE_BLINK:
				case MODE_DELAY_OFF:
				{
					CtrlOff(++port);
					break;
				}
				case MODE_OFF:
				{
					CtrlOn(++port);
					break;
				}
				default:
				{
					break;
				}
			}
			
			return;
		}
		
		// Выставить состояние порта
		// uint8_t port - Номер порта, начиная с 1
		// uint8_t state - Состояние порта
		void CtrlWrite(uint8_t port, state_t state)
		{
			if(state == STATE_OFF)
				CtrlOff(port);
			else
				CtrlOn(port);
			
			return;
		}
		
		// Получить значение тока порта
		// uint8_t port - Номер порта, начиная с 1
		uint16_t GetCurrent(uint8_t port)
		{
			if(--port >= _ports_max) return 0;
			
			channel_t &channel = _channels[port];
			uint16_t current = _CallCurrent(channel);
			
			return current;
		}
		
		// Получить сумму токов всех портов
		uint16_t GetCurrentAll()
		{
			uint16_t result = 0;
			
			for(channel_t &channel : _channels)
			{
				result += _CallCurrent(channel);
			}
			
			return result;
		}
		
		// Получить режим работы порта
		// uint8_t port - Номер порта, начиная с 1	
		mode_t GetMode(uint8_t port)
		{
			if(--port >= _ports_max) return MODE_OFF;
			
			channel_t &channel = _channels[port];
			mode_t mode = channel.mode;
			
			return mode;
		}
		
		// Получить состоние порта
		// uint8_t port - Номер порта, начиная с 1	
		state_t GetState(uint8_t port)
		{
			if(--port >= _ports_max) return STATE_OFF;
			
			channel_t &channel = _channels[port];
			state_t state = channel.state;
			
			return state;
		}
		
		void Processing(uint32_t current_time)
		{
			if(current_time - _last_tick_time < _tick_time) return;
			_last_tick_time = current_time;
			
			for(uint8_t i = 0; i < _ports_max; ++i)
			{
				uint8_t port = i + 1;
				channel_t &channel = _channels[i];
				
				if(channel.mode <= MODE_OFF) continue;
				
				if(channel.current_limit > 0)
				{
					uint16_t current = _CallCurrent(channel);
					if(_CheckCurrent(channel, current) == 1)
					{
						_CallControl(channel, STATE_OFF);
						
						if(_CallbackCurrentLimit != nullptr)
							_CallbackCurrentLimit(port, current);
					}
				}
				
				if(channel.mode == MODE_BLINK && current_time - channel.last_time >= channel.blink_delay)
				{
					channel.last_time = current_time;
					
					if(channel.state == STATE_OFF)
					{
						_CallControl(channel, STATE_ON);
						channel.blink_delay = channel.blink_on;
					}
					else
					{
						_CallControl(channel, STATE_OFF);
						channel.blink_delay = channel.blink_off;
					}
				}
				
				if(channel.mode == MODE_DELAY_OFF && current_time - channel.last_time >= channel.off_delay)
				{
					CtrlOff(port);
				}
			}
			
			return;
		}
		
	private:
		
		struct channel_t
		{
			uint8_t control_id;		// ID для управления портом в callback_control_t
			uint8_t current_id;		// ID для чтения ADC в callback_current_t
			uint16_t current_limit;	// Лимит тока порта, мА

			mode_t mode;			// Текущий режим работы порта
			state_t state;			// Текущее состояние порта
			uint16_t blink_on;		// Время вкл состояния порта в MODE_BLINK, мс
			uint16_t blink_off;		// Время выкл состояния порта в MODE_BLINK, мс
			uint16_t blink_delay;	// Задерка активной фазы порта в MODE_BLINK, мс
			uint16_t off_delay;		// Время через которое нужно отключить порт, мс
			uint32_t last_time;		// Время изменения состояния порта логикой
		};
		
		int8_t _CheckCurrent(channel_t &channel, uint16_t current)
		{
			if(current < 50) return -1;
			else if(current > channel.current_limit) return 1;
			else return 0;
		}
		
		void _CallControl(channel_t &channel, state_t state)
		{
			_CallbackControl(channel.control_id, state);
			channel.state = state;
			
			return;
		}
		
		uint16_t _CallCurrent(channel_t &channel)
		{
			uint16_t current = _CallbackCurrent(channel.current_id);
			
			return current;
		}
		
		tick_provider _Tick;
		callback_control_t _CallbackControl;
		callback_current_t _CallbackCurrent;
		callback_current_limit_t _CallbackCurrentLimit = nullptr;
		
		channel_t _channels[_ports_max];
		
		uint32_t _last_tick_time = 0;
};
