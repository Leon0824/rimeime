#pragma once

#include <WeaselCommon.h>

namespace weasel
{

	struct UIStyle
	{
		std::wstring font_face;
		int font_point;
		// layout
		int min_width;
		int min_height;
		int border;
		int margin_x;
		int margin_y;
		int spacing;
		int candidate_spacing;
		int hilite_spacing;
		int hilite_padding;
		int round_corner;
		// color scheme
		int text_color;
		int candidate_text_color;
		int back_color;
		int border_color;
		int hilited_text_color;
		int hilited_back_color;
		int hilited_candidate_text_color;
		int hilited_candidate_back_color;

		UIStyle() : font_face(), 
			        font_point(0),
					min_width(0),
					min_height(0),
					border(0),
					margin_x(0),
					margin_y(0),
					spacing(0),
					candidate_spacing(0),
					hilite_spacing(0),
					hilite_padding(0),
					round_corner(0),
					text_color(0),
					candidate_text_color(0),
					back_color(0),
					border_color(0),
					hilited_text_color(0),
					hilited_back_color(0),
					hilited_candidate_text_color(0),
					hilited_candidate_back_color(0) {}
	};

	class UIImpl;

	//
	// ���뷨����ӿ���
	//
	class UI
	{
	public:
		UI() : pimpl_(0)
		{
		}

		virtual ~UI()
		{
			if (pimpl_)
				Destroy();
		}

		// �������뷨����
		bool Create(HWND parent);

		// ���ٽ���
		void Destroy();
		
		// ��������
		void Show();
		void Hide();
		
		// ������ʽ
		UIStyle* GetStyle() const;

		// �����뽹��λ�ã�������ʱ�ƶ���ѡ���������ػ�
		void UpdateInputPosition(RECT const& rc);

		// �ػ�
		void Refresh();

		// ���½�����ʾ����
		void Update(weasel::Context const& ctx, weasel::Status const& status);

	private:
		UIImpl* pimpl_;
	};

}