# XP-TUBE

**XP-TUBE** is an independently developed PlayStation 4 media application project. The current work focuses on replacing inherited platform and application boundaries with XP-owned, testable components while preserving third-party licenses and notices.

**XP-TUBE** مشروع تطبيق وسائط مستقل قيد التطوير لمنصة PlayStation 4. يركّز العمل الحالي على استبدال حدود المنصة والتطبيق الموروثة بمكوّنات مملوكة لمشروع XP وقابلة للاختبار، مع الحفاظ على تراخيص المكوّنات الخارجية وإشعاراتها.

## Project status | حالة المشروع

| Area | English | العربية |
|---|---|---|
| XP Core | Implemented and integrated through transitional adapters; some inherited JSON handling remains. | نُفّذ ودُمج عبر طبقات توافق انتقالية؛ وما زالت معالجة JSON تعتمد جزئياً على المكوّن الموروث. |
| XP Network | XP-owned HTTP and certificate-inspection implementation; integration remains transitional. | تنفيذ HTTP وفحص الشهادات مملوك لـ XP؛ وما زال الدمج انتقالياً. |
| XP Platform | Runtime/event queue, PS4 system-service adapter, and language mapping are implemented. | نُفّذ وقت التشغيل وطابور الأحداث ومحوّل خدمات نظام PS4 وتحويل معرّفات اللغات. |
| PS4 input | SDL still owns controller discovery and polling; migration to XP contracts is in progress. | ما زالت SDL مسؤولة عن اكتشاف وحدات التحكم وقراءة أحداثها؛ ونقل هذه المسؤولية إلى عقود XP جارٍ. |
| Display and audio | Display/presentation, audio focus, and system dialogs remain in the migration layer. | ما زالت الشاشة والعرض وتركيز الصوت وحوارات النظام ضمن طبقة الترحيل. |
| UI and services | Existing screens and service integrations have not yet been independently replaced. | لم تُستبدل الشاشات وطبقات الخدمات الحالية بتنفيذ مستقل بعد. |
| Playback | The application playback integration remains migration-derived and uses third-party media components. | ما زال دمج تشغيل الوسائط مشتقاً من المصدر المرحّل ويستخدم مكوّنات وسائط خارجية. |
| Build independence | The build still prepares the vendored migration source using a patching script. | ما زال البناء يجهّز مصدر الترحيل المضمّن باستخدام سكربت لتطبيق التعديلات. |
| PS4 hardware validation | A successful CI build is not a substitute for testing on a real console; hardware validation is still required. | نجاح البناء الآلي لا يعوّض الاختبار على جهاز حقيقي؛ وما زال اختبار الجهاز مطلوباً. |

## Current development sequence | تسلسل التطوير الحالي

1. Complete the XP-owned platform boundary: controller/input, display/presentation, audio focus, and dialogs.
   استكمال حدود المنصة المملوكة لـ XP: وحدة التحكم والإدخال، الشاشة والعرض، تركيز الصوت، وحوارات النظام.
2. Move application services and account/session interfaces onto XP-owned contracts.
   نقل خدمات التطبيق وواجهات الحساب والجلسة إلى عقود مملوكة لـ XP.
3. Build the independent XP UI and playback layers in testable stages.
   بناء طبقتي واجهة XP ومحرك التشغيل المستقلتين على مراحل قابلة للاختبار.
4. Replace the source-patching workflow with maintained repository-owned source.
   استبدال آلية تطبيق التعديلات بمصدر يحتفظ به المشروع ويطوّره مباشرة.
5. Run feature regression, security and licensing checks, then validate on real PS4 hardware.
   إجراء اختبارات تراجع الميزات والأمان والتراخيص، ثم التحقق على جهاز PS4 حقيقي.

## Build and verification | البناء والتحقق

GitHub Actions workflows provide host unit tests and an isolated PS4 package build. Check the workflow result and artifact for the exact commit being evaluated. A package build alone does not establish that the application runs correctly on console hardware.

توفر GitHub Actions مسارات لاختبارات الوحدات على بيئة المضيف وبناء حزمة PS4 معزولة. يجب التحقق من نتيجة المسار والملف الناتج للالتزام البرمجي نفسه. نجاح إنشاء الحزمة وحده لا يثبت أن التطبيق يعمل بصورة صحيحة على جهاز PS4.

## Licensing | التراخيص

This project includes and adapts third-party software. The GPL-3.0 license, third-party license texts, copyright notices, and required attributions must remain with the corresponding source and release archives. XP-TUBE branding does not transfer ownership of inherited or third-party code.

يتضمن المشروع برمجيات خارجية ويجري تعديلها. يجب الإبقاء على ترخيص GPL-3.0 ونصوص تراخيص الأطراف الخارجية وإشعارات حقوق النشر والإسنادات المطلوبة مع المصدر وحزم الإصدار ذات الصلة. لا تعني هوية XP-TUBE امتلاك الشيفرة الموروثة أو شيفرات الأطراف الخارجية.

## Repository layout | بنية المستودع

- `xp/core` — XP-owned core utilities and persistence boundaries.
- `xp/net` — XP-owned network contracts and transport.
- `xp/platform` — platform contracts, runtime, event queue, and PS4 adapters.
- `vendor/playtube-migration` — temporary migration source; not yet removed.
- `scripts/prepare-xp-tube.py` — temporary source preparation/patching step.

- `xp/core` — أدوات XP الأساسية وحدود حفظ البيانات.
- `xp/net` — عقود الشبكة وطبقة النقل المملوكة لـ XP.
- `xp/platform` — عقود المنصة ووقت التشغيل وطابور الأحداث ومحوّلات PS4.
- `vendor/playtube-migration` — مصدر الترحيل المؤقت؛ لم يُحذف بعد.
- `scripts/prepare-xp-tube.py` — خطوة مؤقتة لتجهيز المصدر وتطبيق التعديلات.
