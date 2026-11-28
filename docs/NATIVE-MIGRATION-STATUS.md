# XP-TUBE native migration status | حالة ترحيل XP-TUBE

This document distinguishes XP-owned implementation from inherited compatibility code and from actual console validation.
يوضح هذا المستند الفرق بين التنفيذ المملوك لـ XP وشيفرة التوافق الموروثة والتحقق الفعلي على الجهاز.

## Verified ownership | الملكية الحالية

| Area | English status | الحالة بالعربية |
|---|---|---|
| XP Core | XP-owned utilities and bounded persistence are integrated through transitional adapters; JSON handling remains partly inherited. | أدوات أساسية وحفظ بيانات محدود مملوكان لـ XP ومدمجان عبر محولات انتقالية؛ وما زالت معالجة JSON موروثة جزئياً. |
| XP Network | XP-owned HTTP transport and certificate inspection; application integration is still transitional. | نقل HTTP وفحص الشهادات مملوكان لـ XP؛ وما زال دمج التطبيق انتقالياً. |
| XP Platform runtime | Existing SDL event stream is routed through XP Runtime/EventQueue without a second SDL poll. | يمر تدفق SDL الحالي عبر XP Runtime/EventQueue دون قراءة SDL مرتين. |
| PS4 system services | OpenOrbis bootstrap, language ID, timezone query, and language mapping are XP-owned. | تهيئة OpenOrbis وقراءة اللغة والمنطقة الزمنية وتحويل معرّف اللغة مملوكة لـ XP. |
| Controller/input | SDL still discovers, opens, and polls controllers. XP controller identity/lifecycle primitives are being introduced as a safe intermediate boundary. | ما زالت SDL تكتشف وحدات التحكم وتفتحها وتقرأها. يجري إدخال أدوات XP لهوية وحدة التحكم ودورة اتصالها كحدّ انتقالي آمن. |
| Display/audio/dialogs | Still inherited. | ما زالت موروثة. |
| UI/services/playback | Not independently replaced. | لم تُستبدل بتنفيذ مستقل بعد. |
| Build source | `scripts/prepare-xp-tube.py` still patches `vendor/playtube-migration`. | ما زال `scripts/prepare-xp-tube.py` يطبق تعديلات على `vendor/playtube-migration`. |
| Hardware validation | No real-PS4 validation is established by host tests or CI packaging. | لا تثبت اختبارات المضيف أو إنشاء الحزمة نجاح التشغيل على جهاز PS4 حقيقي. |

## Migration sequence | تسلسل الترحيل

1. Finish XP-owned platform contracts: controller/input, display/presentation, audio focus, and dialogs.
   استكمال عقود المنصة المملوكة لـ XP: وحدة التحكم والإدخال، الشاشة والعرض، تركيز الصوت، وحوارات النظام.
2. Migrate application services and account/session interfaces.
   ترحيل خدمات التطبيق وواجهات الحساب والجلسة.
3. Migrate UI and playback in separately testable stages.
   ترحيل الواجهة ومحرك التشغيل على مراحل مستقلة قابلة للاختبار.
4. Replace source patching with maintained repository-owned source.
   استبدال تطبيق التعديلات بمصدر يحتفظ به المشروع مباشرة.
5. Run feature regression, security, licensing, and real-PS4 checks.
   إجراء اختبارات تراجع الميزات والأمان والتراخيص والتحقق على جهاز PS4 حقيقي.

## Important boundary | حدّ مهم

A successful CI build proves that the configured build completed for that commit. It does not prove runtime correctness on a PS4. Do not remove the migration source or its patching script until all required behavior has an XP-owned replacement and regression coverage.

يثبت نجاح البناء الآلي اكتمال عملية البناء المضبوطة لذلك الالتزام البرمجي، ولا يثبت صحة التشغيل على PS4. لا تحذف مصدر الترحيل أو سكربت التعديلات قبل توفير بديل مملوك لـ XP لكل السلوكيات المطلوبة وتغطيتها باختبارات التراجع.
